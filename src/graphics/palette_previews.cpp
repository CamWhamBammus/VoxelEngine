#include "graphics/palette_previews.h"
#include "graphics/block_relief.h"
#include "graphics/materials.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

namespace voxel {
namespace {

constexpr const char* vertexShader = R"glsl(#version 410 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;
layout(location = 3) in float material;
layout(location = 4) in vec4 relief;
uniform mat4 viewProjection;
out vec3 worldPosition;
out vec3 surfaceNormal;
out vec3 surfaceColor;
out vec4 reliefSurface;
flat out int materialId;
void main() {
    worldPosition = position;
    surfaceNormal = normal;
    surfaceColor = color;
    reliefSurface = relief;
    materialId = int(material + 0.5);
    gl_Position = viewProjection * vec4(position, 1.0);
}
)glsl";

const std::string fragmentShader = std::string(R"glsl(#version 410 core
in vec3 worldPosition;
in vec3 surfaceNormal;
in vec3 surfaceColor;
in vec4 reliefSurface;
flat in int materialId;
uniform int decorative;
uniform float timeSeconds;
uniform vec3 cameraPosition;
out vec4 fragmentColor;
)glsl") + materialShader + R"glsl(
void main() {
    MaterialSurface surface = evaluateMaterial();
    vec3 normal = normalize(surface.normal);
    vec3 key = normalize(vec3(-0.35, 0.85, 0.48));
    vec3 fill = normalize(vec3(0.75, 0.35, -0.5));
    float diffuse = 0.34 + 0.58 * max(dot(normal, key), 0.0)
                         + 0.19 * max(dot(normal, fill), 0.0);
    vec3 view = normalize(cameraPosition - worldPosition);
    float specular = pow(max(dot(normal, normalize(view + key)), 0.0),
                         mix(56.0, 8.0, surface.roughness)) * (1.0 - surface.roughness) * 0.16;
    vec3 color = max(surface.albedo * (diffuse + surface.emission * 0.70) + specular, vec3(0.0));
    // Preserve luminous colors instead of clipping amber glass to white. Gamma
    // is applied once here; the atlas itself is an ordinary linear RGBA8 texture.
    float peak = max(color.r, max(color.g, color.b));
    color /= max(1.0, peak);
    fragmentColor = vec4(pow(clamp(color, 0.0, 1.0), vec3(1.0 / 2.2)), 1.0);
}
)glsl";

// Atlas work can happen inside a HUD draw. Its framebuffer, depth setup, and
// uploads must leave the enclosing renderer exactly as they found it.
struct SavedState {
    GLint drawFrame=0, readFrame=0, renderbuffer=0, program=0, vao=0, arrayBuffer=0;
    GLint unpackBuffer=0, activeTexture=0, texture=0, depthFunction=0, cullMode=0, frontFace=0;
    std::array<GLint,4> viewport{}, scissor{};
    std::array<GLint,2> polygon{};
    std::array<GLboolean,4> colorMask{};
    GLboolean depthMask=GL_TRUE;
    std::array<GLfloat,4> clearColor{};
    GLdouble clearDepth=1;
    std::array<GLdouble,2> depthRange{};
    static constexpr std::array<GLenum,13> capabilities{
        GL_DEPTH_TEST, GL_CULL_FACE, GL_BLEND, GL_SCISSOR_TEST, GL_STENCIL_TEST,
        GL_FRAMEBUFFER_SRGB, GL_POLYGON_OFFSET_FILL, GL_RASTERIZER_DISCARD, GL_DITHER,
        GL_COLOR_LOGIC_OP, GL_MULTISAMPLE, GL_SAMPLE_ALPHA_TO_COVERAGE, GL_SAMPLE_COVERAGE};
    std::array<GLboolean,capabilities.size()> enabled{};

    SavedState() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&drawFrame);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFrame);
        glGetIntegerv(GL_RENDERBUFFER_BINDING,&renderbuffer);glGetIntegerv(GL_CURRENT_PROGRAM,&program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&arrayBuffer);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpackBuffer);glGetIntegerv(GL_ACTIVE_TEXTURE,&activeTexture);
        glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
        glGetIntegerv(GL_DEPTH_FUNC,&depthFunction);glGetIntegerv(GL_CULL_FACE_MODE,&cullMode);
        glGetIntegerv(GL_FRONT_FACE,&frontFace);glGetIntegerv(GL_VIEWPORT,viewport.data());
        glGetIntegerv(GL_SCISSOR_BOX,scissor.data());glGetIntegerv(GL_POLYGON_MODE,polygon.data());
        glGetBooleanv(GL_COLOR_WRITEMASK,colorMask.data());glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
        glGetFloatv(GL_COLOR_CLEAR_VALUE,clearColor.data());glGetDoublev(GL_DEPTH_CLEAR_VALUE,&clearDepth);
        glGetDoublev(GL_DEPTH_RANGE,depthRange.data());
        for (std::size_t i=0;i<capabilities.size();++i) enabled[i]=glIsEnabled(capabilities[i]);
    }
    ~SavedState() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,drawFrame);glBindFramebuffer(GL_READ_FRAMEBUFFER,readFrame);
        glBindRenderbuffer(GL_RENDERBUFFER,renderbuffer);glUseProgram(program);
        glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,arrayBuffer);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,unpackBuffer);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);glActiveTexture(activeTexture);
        glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
        glScissor(scissor[0],scissor[1],scissor[2],scissor[3]);
        glDepthFunc(depthFunction);glDepthMask(depthMask);glDepthRange(depthRange[0],depthRange[1]);
        glCullFace(cullMode);glFrontFace(frontFace);glPolygonMode(GL_FRONT_AND_BACK,polygon[0]);
        glColorMask(colorMask[0],colorMask[1],colorMask[2],colorMask[3]);
        glClearColor(clearColor[0],clearColor[1],clearColor[2],clearColor[3]);glClearDepth(clearDepth);
        for (std::size_t i=0;i<capabilities.size();++i)
            if (enabled[i]) glEnable(capabilities[i]); else glDisable(capabilities[i]);
    }
};

struct PreviewMesh { MeshData model, relief; int decorative=0; };
PreviewMesh previewMesh(int id)
{
    PreviewMesh result;
    if (id < static_cast<int>(BlockCount)) {
        const auto block=static_cast<Block>(id);
        if (block==Block::Air) return result;
        // This empty upper chunk avoids generating a separate full world for
        // each block. Coordinates remain unchanged for real material/relief UVs.
        static World sampleWorld;
        constexpr glm::ivec3 cell{0,24,0};
        constexpr std::size_t chunk=World::Radius+World::Width*(World::Radius+World::Width);
        Chunk cube;cube.set(0,0,0,block);
        result.model=buildMesh(cube,cell);
        sampleWorld.set(cell,block);
        result.relief=buildBlockRelief(sampleWorld,chunk);
        sampleWorld.set(cell,Block::Air);
    } else {
        result.decorative=1;
        HubAsset asset{HubAssetKind::ItemPickup,{},0,0,{}, {ItemId::Coal,1}};
        if (id < static_cast<int>(BlockCount+HubAssetCount)) {
            asset.kind=static_cast<HubAssetKind>(id-BlockCount);
            if (asset.kind!=HubAssetKind::ItemPickup) asset.pickupItem={};
        } else asset.pickupItem.item=static_cast<ItemId>(id-BlockCount-HubAssetCount);
        result.model=buildHubAsset(asset);
    }
    return result;
}

glm::mat4 projectionFor(const PreviewMesh& mesh, glm::vec3& camera)
{
    glm::vec3 low(std::numeric_limits<float>::max()),high(std::numeric_limits<float>::lowest());
    const auto bounds=[&](const MeshData& part) {
        for (const auto& vertex:part.vertices) {low=glm::min(low,vertex.position);high=glm::max(high,vertex.position);}
    };
    bounds(mesh.model);bounds(mesh.relief);
    const auto center=(low+high)*.5f;
    const float distance=glm::length(high-low)*2+1;
    camera=center+glm::normalize(glm::vec3(1.25f,1.05f,1.6f))*distance;
    const auto view=glm::lookAt(camera,center,glm::vec3(0,1,0));
    glm::vec2 min(std::numeric_limits<float>::max()),max(std::numeric_limits<float>::lowest());
    const auto screenBounds=[&](const MeshData& part) {
        for (const auto& vertex:part.vertices) {
            const auto p=glm::vec2(view*glm::vec4(vertex.position,1));
            min=glm::min(min,p);max=glm::max(max,p);
        }
    };
    screenBounds(mesh.model);screenBounds(mesh.relief);
    const auto middle=(min+max)*.5f;
    const float extent=std::max({max.x-min.x,max.y-min.y,.01f})*.5f*128.0f/110.0f;
    return glm::ortho(middle.x-extent,middle.x+extent,middle.y-extent,middle.y+extent,.01f,distance*3)*view;
}

} // namespace

glm::vec4 PalettePreviews::uvBounds(int id)
{
    if (id<0 || id>=Count) return {};
    const float x=static_cast<float>((id%Columns)*TileSize),y=static_cast<float>((id/Columns)*TileSize);
    return {x/Width,y/Height,(x+TileSize)/Width,(y+TileSize)/Height};
}

PalettePreviews::PalettePreviews() : shader_(vertexShader,fragmentShader.c_str())
{
    glGenTextures(1,&texture_);glGenFramebuffers(1,&framebuffer_);glGenRenderbuffers(1,&depth_);
    if (!texture_ || !framebuffer_ || !depth_) {
        glDeleteTextures(1,&texture_);glDeleteFramebuffers(1,&framebuffer_);glDeleteRenderbuffers(1,&depth_);
        throw std::runtime_error("Failed to allocate palette preview resources");
    }
}

PalettePreviews::~PalettePreviews()
{
    glDeleteTextures(1,&texture_);glDeleteFramebuffers(1,&framebuffer_);glDeleteRenderbuffers(1,&depth_);
}

void PalettePreviews::allocate()
{
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
    glBindTexture(GL_TEXTURE_2D,texture_);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,Width,Height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_BASE_LEVEL,0);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,0);
    glBindRenderbuffer(GL_RENDERBUFFER,depth_);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,Width,Height);
    glBindFramebuffer(GL_FRAMEBUFFER,framebuffer_);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture_,0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth_);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);glReadBuffer(GL_COLOR_ATTACHMENT0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE || glGetError()!=GL_NO_ERROR)
        throw std::runtime_error("Could not allocate the palette preview atlas");
    glDisable(GL_SCISSOR_TEST);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    allocated_=true;
}

void PalettePreviews::drawTile(int id)
{
    const int x=(id%Columns)*TileSize,y=(id/Columns)*TileSize;
    glViewport(x,y,TileSize,TileSize);glScissor(x,y,TileSize,TileSize);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    const auto preview=previewMesh(id);
    if (preview.model.indices.empty()) return;
    glm::vec3 camera;
    shader_.setMat4("viewProjection",projectionFor(preview,camera));
    shader_.setVec3("cameraPosition",camera);shader_.setFloat("timeSeconds",0);
    mesh_.upload(preview.model);relief_.upload(preview.relief);
    shader_.setInt("decorative",2);relief_.draw();
    shader_.setInt("decorative",preview.decorative);mesh_.draw();
}

void PalettePreviews::ensure(std::span<const int> ids)
{
    if (std::none_of(ids.begin(),ids.end(),[&](int id){return id>=0 && id<Count && !ready_[id];})) return;
    SavedState previous;
    for (const auto capability:SavedState::capabilities) glDisable(capability);
    glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);
    glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);glDepthRange(0,1);
    glCullFace(GL_BACK);glFrontFace(GL_CCW);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(0,0,0,0);glClearDepth(1);
    if (!allocated_) allocate();
    glBindFramebuffer(GL_FRAMEBUFFER,framebuffer_);glEnable(GL_SCISSOR_TEST);
    shader_.use();
    for (const int id:ids) {
        if (id<0 || id>=Count || ready_[id]) continue;
        drawTile(id);ready_[id]=true;++generatedCount_;
    }
}

} // namespace voxel
