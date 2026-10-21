#include "palette_preview_smoke.h"
#include "graphics/palette_previews.h"
#include "sections.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace voxel {
namespace {
void require(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(message);
}

struct State {
    std::vector<GLint> bindings;
    std::array<GLint,4> viewport{}, scissor{}, textures{};
    std::array<GLint,2> polygon{};
    std::array<GLfloat,4> clearColor{};
    GLdouble clearDepth = 0;
    std::array<GLboolean,4> colorMask{};
    GLboolean depthMask = GL_FALSE;
    std::array<GLboolean,9> enabled{};
    bool operator==(const State&) const = default;
};
State state()
{
    State result;
    constexpr std::array<GLenum,22> names{
        GL_CURRENT_PROGRAM,GL_VERTEX_ARRAY_BINDING,GL_ARRAY_BUFFER_BINDING,GL_ELEMENT_ARRAY_BUFFER_BINDING,
        GL_DRAW_FRAMEBUFFER_BINDING,GL_READ_FRAMEBUFFER_BINDING,GL_RENDERBUFFER_BINDING,
        GL_ACTIVE_TEXTURE,GL_DEPTH_FUNC,GL_CULL_FACE_MODE,GL_FRONT_FACE,
        GL_BLEND_SRC_RGB,GL_BLEND_DST_RGB,GL_BLEND_SRC_ALPHA,GL_BLEND_DST_ALPHA,
        GL_BLEND_EQUATION_RGB,GL_BLEND_EQUATION_ALPHA,GL_PACK_ALIGNMENT,GL_UNPACK_ALIGNMENT,
        GL_DRAW_BUFFER,GL_READ_BUFFER,GL_STENCIL_CLEAR_VALUE};
    for (auto name : names) {GLint value=0;glGetIntegerv(name,&value);result.bindings.push_back(value);}
    glGetIntegerv(GL_VIEWPORT,result.viewport.data());glGetIntegerv(GL_SCISSOR_BOX,result.scissor.data());
    glGetIntegerv(GL_POLYGON_MODE,result.polygon.data());glGetFloatv(GL_COLOR_CLEAR_VALUE,result.clearColor.data());
    glGetDoublev(GL_DEPTH_CLEAR_VALUE,&result.clearDepth);glGetBooleanv(GL_COLOR_WRITEMASK,result.colorMask.data());
    glGetBooleanv(GL_DEPTH_WRITEMASK,&result.depthMask);
    constexpr std::array<GLenum,9> modes{GL_DEPTH_TEST,GL_CULL_FACE,GL_BLEND,GL_SCISSOR_TEST,GL_DITHER,
        GL_MULTISAMPLE,GL_FRAMEBUFFER_SRGB,GL_POLYGON_OFFSET_FILL,GL_STENCIL_TEST};
    for (std::size_t i=0;i<modes.size();++i) result.enabled[i]=glIsEnabled(modes[i]);
    GLint active=0;glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
    for (int unit=0;unit<4;++unit) {glActiveTexture(GL_TEXTURE0+unit);glGetIntegerv(GL_TEXTURE_BINDING_2D,&result.textures[unit]);}
    glActiveTexture(active);
    return result;
}
using Pixel = std::array<unsigned char,4>;
using Tile = std::vector<Pixel>;

Tile tile(const std::vector<Pixel>& pixels, int id)
{
    const auto uv=PalettePreviews::uvBounds(id);
    require(uv.x>=0 && uv.y>=0 && uv.z<=1 && uv.w<=1 && uv.x<uv.z && uv.y<uv.w,
            "Preview UV bounds must stay within the atlas");
    const int left=static_cast<int>(std::lround(uv.x*PalettePreviews::Width));
    const int bottom=static_cast<int>(std::lround(uv.y*PalettePreviews::Height));
    const int right=static_cast<int>(std::lround(uv.z*PalettePreviews::Width));
    const int top=static_cast<int>(std::lround(uv.w*PalettePreviews::Height));
    require(right-left==PalettePreviews::TileSize && top-bottom==PalettePreviews::TileSize,
            "Each preview must occupy one complete atlas tile");
    Tile result;
    for (int y=bottom;y<top;++y)
        for (int x=left;x<right;++x) result.push_back(pixels[static_cast<std::size_t>(y)*PalettePreviews::Width+x]);
    return result;
}
std::size_t coverage(const Tile& pixels)
{
    return std::count_if(pixels.begin(),pixels.end(),[](const auto& p){return p[3]>127;});
}
std::size_t silhouetteDifference(const Tile& a,const Tile& b)
{
    std::size_t result=0;
    for (std::size_t i=0;i<a.size();++i) result+=(a[i][3]>127)!=(b[i][3]>127);
    return result;
}
}

void verifyPalettePreviews()
{
    Shader caller("#version 410 core\nvoid main(){gl_Position=vec4(0);}",
                  "#version 410 core\nout vec4 color;void main(){color=vec4(1);}");
    GLuint vao=0,buffer=0,renderbuffer=0;
    std::array<GLuint,2> frames{},textures{};
    glGenVertexArrays(1,&vao);glGenBuffers(1,&buffer);glGenRenderbuffers(1,&renderbuffer);
    glGenFramebuffers(2,frames.data());glGenTextures(2,textures.data());
    const auto cleanup=[&] {
        glUseProgram(0);glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);
        glBindFramebuffer(GL_FRAMEBUFFER,0);glBindRenderbuffer(GL_RENDERBUFFER,0);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,0);
        glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE0);
        glDeleteTextures(2,textures.data());glDeleteFramebuffers(2,frames.data());
        glDeleteRenderbuffers(1,&renderbuffer);glDeleteBuffers(1,&buffer);glDeleteVertexArrays(1,&vao);
        glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);glDisable(GL_STENCIL_TEST);
        glDisable(GL_POLYGON_OFFSET_FILL);glDisable(GL_FRAMEBUFFER_SRGB);glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        glFrontFace(GL_CCW);glCullFace(GL_BACK);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        glBlendEquation(GL_FUNC_ADD);glClearDepth(1);glPixelStorei(GL_PACK_ALIGNMENT,4);glPixelStorei(GL_UNPACK_ALIGNMENT,4);
    };
    try {
        caller.use();glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,buffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,frames[0]);glBindFramebuffer(GL_READ_FRAMEBUFFER,frames[1]);
        glBindRenderbuffer(GL_RENDERBUFFER,renderbuffer);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,textures[0]);
        glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,textures[1]);
        glViewport(7,9,31,37);glEnable(GL_SCISSOR_TEST);glScissor(2,3,4,5);
        glEnable(GL_DEPTH_TEST);glDepthFunc(GL_GEQUAL);glDepthMask(GL_FALSE);
        glEnable(GL_CULL_FACE);glCullFace(GL_FRONT);glFrontFace(GL_CW);
        glEnable(GL_BLEND);glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ZERO,GL_ONE);
        glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT,GL_FUNC_ADD);
        glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);glColorMask(GL_TRUE,GL_FALSE,GL_TRUE,GL_FALSE);
        glClearColor(.17f,.23f,.31f,.41f);glClearDepth(.23);
        glPixelStorei(GL_PACK_ALIGNMENT,8);glPixelStorei(GL_UNPACK_ALIGNMENT,8);
        const auto before=state();
        PalettePreviews previews;
        require(state()==before,"Constructing palette previews changed caller GL state");
        require(previews.generatedCount()==0,"Palette previews must not render before they are requested");
        const std::array first{PalettePreviews::blockId(Block::Grass),PalettePreviews::assetId(HubAssetKind::Fern)};
        previews.ensure(first);
        require(state()==before && previews.generatedCount()==2,
                "The first lazy request must generate only requested previews and restore caller GL state");
        std::vector<int> ids(PalettePreviews::Count);std::iota(ids.begin(),ids.end(),0);
        previews.ensure(ids);
        require(state()==before,"First atlas generation leaked caller framebuffer, texture, raster, or mask state");
        require(static_cast<std::size_t>(previews.generatedCount())==ids.size(),"Not every requested preview was generated");
        const auto texture=previews.texture();
        previews.ensure(ids);previews.ensure({});
        require(previews.texture()==texture && static_cast<std::size_t>(previews.generatedCount())==ids.size() && state()==before,
                "Repeated palette draws must reuse cached thumbnails without GL state changes");

        glBindFramebuffer(GL_FRAMEBUFFER,0);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);
        std::vector<Pixel> pixels(static_cast<std::size_t>(PalettePreviews::Width)*PalettePreviews::Height);
        glPixelStorei(GL_PACK_ALIGNMENT,1);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        require(glGetError()==GL_NO_ERROR,"Reading the rendered palette atlas caused an OpenGL error");
        for (const auto id:ids) {
            const auto image=tile(pixels,id);
            if (id==PalettePreviews::blockId(Block::Air)) {
                require(coverage(image)==0,"The air preview must remain transparent");continue;
            }
            require(coverage(image)>12,"Empty preview tile "+std::to_string(id));
            constexpr int size=PalettePreviews::TileSize;
            for (int i=0;i<size;++i)
                require(image[i][3]==0 && image[(size-1)*size+i][3]==0
                        && image[i*size][3]==0 && image[i*size+size-1][3]==0,
                        "Preview geometry clips its tile border: "+std::to_string(id));
        }
        const auto grass=tile(pixels,PalettePreviews::blockId(Block::Grass));
        const auto green=std::count_if(grass.begin(),grass.end(),[](const auto& p){return p[3]>127 && p[1]>p[0]*1.12f && p[1]>p[2]*1.12f;});
        const auto earth=std::count_if(grass.begin(),grass.end(),[](const auto& p){return p[3]>127 && p[0]>p[1]*1.12f && p[0]>p[2]*1.2f;});
        require(green>40 && earth>40,"Grass preview must show green turf and brown soil faces");
        const auto stone=tile(pixels,PalettePreviews::blockId(Block::Stone));
        std::set<std::array<unsigned char,3>> colors;
        for (const auto& p:stone) if (p[3]>250) colors.insert({p[0],p[1],p[2]});
        require(colors.size()>32,"Stone preview lost its painted material and became a flat shaded cube");
        for (const auto block:{Block::Leaves,Block::GlowLeaves,Block::BlossomLeaves}) {
            const auto count=coverage(tile(pixels,PalettePreviews::blockId(block)));
            require(count>coverage(grass)/10 && count<coverage(grass)*.95,
                    "Leaf previews must retain real transparent foliage cutouts");
        }
        const auto chest=tile(pixels,PalettePreviews::assetId(HubAssetKind::LootChest));
        const auto fern=tile(pixels,PalettePreviews::assetId(HubAssetKind::Fern));
        const auto arch=tile(pixels,PalettePreviews::assetId(HubAssetKind::RootArch));
        require(silhouetteDifference(chest,fern)>chest.size()/20
                && silhouetteDifference(chest,arch)>chest.size()/20,
                "Asset previews must preserve distinct model silhouettes and open geometry");
        const auto copper=tile(pixels,PalettePreviews::blockId(Block::CopperOre));
        const auto iron=tile(pixels,PalettePreviews::blockId(Block::IronOre));
        std::size_t oreColorDifference=0;
        for (std::size_t i=0;i<copper.size();++i)
            oreColorDifference+=copper[i][3]>127 && iron[i][3]>127
                && std::abs(int(copper[i][0])-int(iron[i][0]))+std::abs(int(copper[i][1])-int(iron[i][1]))
                   +std::abs(int(copper[i][2])-int(iron[i][2]))>24;
        require(oreColorDifference>40,"Ore block previews must show their distinct mineral materials");
    } catch (...) {cleanup();throw;}
    cleanup();
    std::cout<<"Palette previews passed: actual textured meshes, foliage cutouts, transparent padding, cache reuse and GL state\n";
}
} // namespace voxel
