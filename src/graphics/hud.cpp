#include "graphics/hud.h"
#include "game.h"
#include "equipment.h"
#include "root_crossings.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace voxel {
namespace {

constexpr const char* vertexShader = R"glsl(#version 410 core
layout(location = 0) in vec2 position;
layout(location = 1) in vec4 color;
out vec4 pixelColor;
void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    pixelColor = color;
}
)glsl";

constexpr const char* fragmentShader = R"glsl(#version 410 core
in vec4 pixelColor;
out vec4 fragmentColor;
void main() {
    fragmentColor = pixelColor;
}
)glsl";

// Seven rows of five bits, starting at the top-left pixel.
using Glyph = std::array<std::uint8_t, 7>;

Glyph glyph(char character)
{
    if (character >= 'a' && character <= 'z') {
        character = static_cast<char>(character - 'a' + 'A');
    }
    switch (character) {
    case ' ': case '\t': case '\r': return {};
    case 'A': return {14, 17, 17, 31, 17, 17, 17};
    case 'B': return {30, 17, 17, 30, 17, 17, 30};
    case 'C': return {14, 17, 16, 16, 16, 17, 14};
    case 'D': return {30, 17, 17, 17, 17, 17, 30};
    case 'E': return {31, 16, 16, 30, 16, 16, 31};
    case 'F': return {31, 16, 16, 30, 16, 16, 16};
    case 'G': return {14, 17, 16, 23, 17, 17, 15};
    case 'H': return {17, 17, 17, 31, 17, 17, 17};
    case 'I': return {14, 4, 4, 4, 4, 4, 14};
    case 'J': return {7, 2, 2, 2, 18, 18, 12};
    case 'K': return {17, 18, 20, 24, 20, 18, 17};
    case 'L': return {16, 16, 16, 16, 16, 16, 31};
    case 'M': return {17, 27, 21, 21, 17, 17, 17};
    case 'N': return {17, 25, 25, 21, 19, 19, 17};
    case 'O': return {14, 17, 17, 17, 17, 17, 14};
    case 'P': return {30, 17, 17, 30, 16, 16, 16};
    case 'Q': return {14, 17, 17, 17, 21, 18, 13};
    case 'R': return {30, 17, 17, 30, 20, 18, 17};
    case 'S': return {15, 16, 16, 14, 1, 1, 30};
    case 'T': return {31, 4, 4, 4, 4, 4, 4};
    case 'U': return {17, 17, 17, 17, 17, 17, 14};
    case 'V': return {17, 17, 17, 17, 17, 10, 4};
    case 'W': return {17, 17, 17, 21, 21, 21, 10};
    case 'X': return {17, 17, 10, 4, 10, 17, 17};
    case 'Y': return {17, 17, 10, 4, 4, 4, 4};
    case 'Z': return {31, 1, 2, 4, 8, 16, 31};
    case '0': return {14, 17, 19, 21, 25, 17, 14};
    case '1': return {4, 12, 4, 4, 4, 4, 14};
    case '2': return {14, 17, 1, 2, 4, 8, 31};
    case '3': return {30, 1, 1, 14, 1, 1, 30};
    case '4': return {2, 6, 10, 18, 31, 2, 2};
    case '5': return {31, 16, 16, 30, 1, 1, 30};
    case '6': return {14, 16, 16, 30, 17, 17, 14};
    case '7': return {31, 1, 2, 4, 8, 8, 8};
    case '8': return {14, 17, 17, 14, 17, 17, 14};
    case '9': return {14, 17, 17, 15, 1, 1, 14};
    case '.': return {0, 0, 0, 0, 0, 12, 12};
    case ',': return {0, 0, 0, 0, 0, 4, 8};
    case ':': return {0, 4, 4, 0, 4, 4, 0};
    case ';': return {0, 4, 4, 0, 4, 4, 8};
    case '!': return {4, 4, 4, 4, 4, 0, 4};
    case '-': return {0, 0, 0, 31, 0, 0, 0};
    case '+': return {0, 4, 4, 31, 4, 4, 0};
    case '=': return {0, 0, 31, 0, 31, 0, 0};
    case '_': return {0, 0, 0, 0, 0, 0, 31};
    case '/': return {1, 2, 2, 4, 8, 8, 16};
    case '\\': return {16, 8, 8, 4, 2, 2, 1};
    case '|': return {4, 4, 4, 4, 4, 4, 4};
    case '(': return {2, 4, 8, 8, 8, 4, 2};
    case ')': return {8, 4, 2, 2, 2, 4, 8};
    case '[': return {14, 8, 8, 8, 8, 8, 14};
    case ']': return {14, 2, 2, 2, 2, 2, 14};
    case '<': return {1, 2, 4, 8, 4, 2, 1};
    case '>': return {16, 8, 4, 2, 4, 8, 16};
    case '%': return {25, 25, 2, 4, 8, 19, 19};
    case '#': return {10, 10, 31, 10, 31, 10, 10};
    case '*': return {0, 21, 14, 31, 14, 21, 0};
    case '"': return {10, 10, 10, 0, 0, 0, 0};
    case '\'': return {4, 4, 8, 0, 0, 0, 0};
    default: return {14, 17, 1, 2, 4, 0, 4};
    }
}

struct Vertex {
    float x;
    float y;
    glm::vec4 color;
};

constexpr glm::vec4 Cream{0.94f,0.93f,0.83f,1};
constexpr glm::vec4 Muted{0.57f,0.70f,0.69f,1};
constexpr glm::vec4 Gold{0.95f,0.70f,0.30f,1};
constexpr glm::vec4 Panel{0.035f,0.075f,0.085f,0.97f};
constexpr glm::vec4 Edge{0.23f,0.37f,0.37f,1};
constexpr glm::vec4 Leaf{0.45f,0.86f,0.60f,1};

// Small colored triangles and bitmap glyphs share one buffer and draw call.
// Coordinates start at the top-left; play UI uses logical pixels for high DPI.
class Drawing {
public:
    Drawing(float width, float height) : width_(width), height_(height) {}
    std::vector<Vertex> vertices;

    void triangle(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec4 color)
    {
        for (const auto p : {a,b,c})
            vertices.push_back({2 * p.x / width_ - 1, 1 - 2 * p.y / height_, color});
    }
    void quad(float x, float y, float width, float height, glm::vec4 color)
    {
        triangle({x,y}, {x+width,y}, {x+width,y+height}, color);
        triangle({x,y}, {x+width,y+height}, {x,y+height}, color);
    }
    void bar(glm::vec2 a, glm::vec2 b, float width, glm::vec4 color)
    {
        const auto d = b - a;
        const auto side = glm::normalize(glm::vec2(-d.y,d.x)) * width * 0.5f;
        triangle(a-side, b-side, b+side, color);
        triangle(a-side, b+side, a+side, color);
    }
    void text(std::string_view value, float x, float y, float pixel, glm::vec4 color = Cream)
    {
        for (const char character : value) {
            const auto bitmap = glyph(character);
            for (int row = 0; row < 7; ++row) for (int column = 0; column < 5; ++column)
                if ((bitmap[row] & (1u << (4-column))) != 0)
                    quad(x + column * pixel, y + row * pixel, pixel, pixel, color);
            x += 6 * pixel;
        }
    }
    void centered(std::string_view value, float center, float y, float pixel, glm::vec4 color = Cream)
    {
        text(value, center - (value.size() * 6 - (value.empty() ? 0 : 1)) * pixel * 0.5f, y, pixel, color);
    }
    void frame(float x, float y, float width, float height, glm::vec4 border, glm::vec4 fill = Panel)
    {
        quad(x,y,width,height,border);
        quad(x+2,y+2,width-4,height-4,fill);
    }

private:
    float width_, height_;
};

void pickaxeIcon(Drawing& out, glm::vec2 center, float scale, int tier)
{
    const auto p = [&](float x, float y) { return center + glm::vec2(x,y) * scale; };
    out.bar(p(-11,14),p(7,-10),5.5f*scale,{0.11f,0.10f,0.08f,1});
    out.bar(p(-11,13),p(7,-10),3.2f*scale,{0.52f,0.32f,0.15f,1});
    out.bar(p(-10,12),p(5,-8),1.0f*scale,{0.81f,0.59f,0.29f,1});
    constexpr std::array<glm::vec2,8> head{{{-14,-6},{-9,-13},{5,-14},{15,-7},
                                         {18,2},{10,-5},{3,-8},{-7,-8}}};
    const auto color = tier <= 1 ? glm::vec4(0.47f,0.59f,0.65f,1)
        : tier == 2 ? glm::vec4(0.84f,0.49f,0.25f,1) : glm::vec4(0.66f,0.83f,0.88f,1);
    for (std::size_t i=1; i+1<head.size(); ++i)
        out.triangle(center+head[0]*scale,center+head[i]*scale,center+head[i+1]*scale,color);
    out.triangle(p(-9,-13),p(5,-14),p(3,-10),Cream);
    out.triangle(p(5,-14),p(15,-7),p(3,-10),color*glm::vec4(0.72f,0.72f,0.72f,1));
    out.bar(p(0,-10),p(4,-5),3.0f*scale,Gold);
}

void lanternIcon(Drawing& out, glm::vec2 center, float scale)
{
    const auto p = [&](float x, float y) { return center + glm::vec2(x,y)*scale; };
    const auto box = [&](float x,float y,float w,float h,glm::vec4 color) {
        out.quad(center.x+x*scale,center.y+y*scale,w*scale,h*scale,color);
    };
    out.bar(p(-5,-11),p(-5,-17),2*scale,Gold);
    out.bar(p(-5,-17),p(5,-17),2*scale,Gold);
    out.bar(p(5,-17),p(5,-11),2*scale,Gold);
    box(-8,-8,16,21,{0.88f,0.41f,0.11f,1});
    box(-5,-6,10,16,{1.0f,0.73f,0.28f,1});
    out.triangle(p(0,-4),p(4,6),p(-3,8),{1.0f,0.94f,0.61f,1});
    out.triangle(p(-11,-8),p(0,-14),p(11,-8),Gold);
    box(-10,-9,20,3,{0.32f,0.25f,0.14f,1});
    box(-10,12,20,4,Gold);
    box(-9,-7,2,20,Edge); box(7,-7,2,20,Edge);
    box(-8,2,16,2,{0.50f,0.35f,0.15f,1});
}

void compassIcon(Drawing& out, glm::vec2 center, float scale, glm::vec2 direction)
{
    for (int ring=0; ring<2; ++ring) {
        const float radius = (ring == 0 ? 17.0f : 14.0f) * scale;
        for (int side=0; side<8; ++side) {
            const float a=side*0.785398163f, b=(side+1)*0.785398163f;
            out.triangle(center,center+glm::vec2(std::cos(a),std::sin(a))*radius,
                         center+glm::vec2(std::cos(b),std::sin(b))*radius,ring==0 ? Gold : Panel);
        }
    }
    for (const auto side : {glm::vec2(0,1),glm::vec2(0,-1),glm::vec2(1,0),glm::vec2(-1,0)})
        out.bar(center+side*10.5f*scale,center+side*13.0f*scale,1.2f*scale,Muted);
    const glm::vec2 across(-direction.y,direction.x);
    out.triangle(center+direction*11.0f*scale,center+across*3.5f*scale,center-across*3.5f*scale,
                 {0.30f,0.89f,0.79f,1});
    out.triangle(center-direction*10.0f*scale,center-across*3.5f*scale,center+across*3.5f*scale,Cream);
    out.quad(center.x-1.5f*scale,center.y-1.5f*scale,3*scale,3*scale,Gold);
}

std::string crossingTime(float seconds)
{
    const int tenths = std::isfinite(seconds)
        ? static_cast<int>(std::clamp(seconds,0.0f,5999.9f)*10.0f) : 0;
    const int minutes = tenths/600, remainder = tenths/10%60;
    return std::to_string(minutes)+":"+(remainder<10 ? "0" : "")
        +std::to_string(remainder)+"."+std::to_string(tenths%10);
}

void wrappedDescription(Drawing& out, std::string_view text, float x, float y,
                        std::size_t columns)
{
    for (int row=0; row<2 && !text.empty(); ++row) {
        std::size_t length=std::min(columns,text.size());
        if (length<text.size()) {
            const auto space=text.rfind(' ',length);
            if (space!=std::string_view::npos && space>0) length=space;
        }
        out.text(text.substr(0,length),x,y+row*14.0f,1,Cream);
        text.remove_prefix(length);
        while (!text.empty() && text.front()==' ') text.remove_prefix(1);
    }
}

void crossingPicker(Drawing& out, const Game& game, float width, float height)
{
    out.quad(0,0,width,height,{0.01f,0.025f,0.035f,0.72f});
    constexpr float panelWidth=578, panelHeight=420;
    const float x=(width-panelWidth)*0.5f, y=(height-panelHeight)*0.5f;
    out.quad(x+6,y+7,panelWidth,panelHeight,{0,0,0,0.35f});
    out.frame(x,y,panelWidth,panelHeight,Edge);
    out.quad(x+2,y+2,panelWidth-4,3,Leaf);
    out.text("ROOT CROSSINGS",x+23,y+18,2,Leaf);
    out.text("LUSH CAVES / 10 PLAYABLE LAYOUTS",x+24,y+43,1,Muted);
    out.text("ESC - CLOSE",x+panelWidth-90,y+22,1,Muted);

    const auto catalog=rootCrossingCatalog();
    const int selection=std::clamp(game.crossingMenuSelection(),0,RootCrossingCount-1);
    constexpr float cardWidth=260, cardHeight=43;
    for (int index=0; index<RootCrossingCount; ++index) {
        const auto& info=catalog[index];
        const bool selected=index==selection;
        const float cardX=x+23+(index/5)*(cardWidth+12);
        const float cardY=y+66+(index%5)*49;
        out.frame(cardX,cardY,cardWidth,cardHeight,selected ? Gold : Edge,
                  selected ? glm::vec4(0.09f,0.19f,0.16f,1) : Panel);
        out.quad(cardX+7,cardY+8,18,25,selected ? Gold : glm::vec4(0.12f,0.22f,0.21f,1));
        out.centered(std::to_string((index+1)%10),cardX+16,cardY+16,1.25f,
                     selected ? Panel : Muted);
        const float namePixel=std::min(1.25f,213.0f/std::max(1.0f,info.name.size()*6.0f));
        out.text(info.name,cardX+33,cardY+9,namePixel,selected ? Cream : Muted);
        out.text(info.difficulty,cardX+33,cardY+27,1,selected ? Leaf : Muted);
    }
    out.quad(x+23,y+316,panelWidth-46,1,Edge);
    wrappedDescription(out,catalog[selection].description,x+24,y+330,88);
    out.text("CYAN CHECKPOINT / GOLD EXIT / FALLS RESET",x+24,y+359,0.9f,Leaf);
    out.text("UP/DOWN OR 1-9/0 - SELECT",x+24,y+376,1,Muted);
    out.text("WASD MOVE / SPACE JUMP / SHIFT SPRINT",x+24,y+395,1,Muted);
    out.frame(x+360,y+370,194,34,Gold,{0.72f,0.49f,0.20f,1});
    out.centered("ENTER - START",x+457,y+382,1.5f,Panel);
}

void crossingProgress(Drawing& out, const Game& game, float width, float height)
{
    const int index=game.rootCrossingIndex();
    if (index<0 || index>=RootCrossingCount) return;
    const auto& info=rootCrossingCatalog()[index];
    constexpr float panelWidth=420;
    const float center=width*0.5f, x=center-panelWidth*0.5f;
    out.frame(x,16,panelWidth,83,Edge);
    out.quad(x+2,18,3,79,Leaf);
    out.text("ROOT CROSSING "+std::to_string(index+1)+" / "+std::to_string(RootCrossingCount),
             x+17,28,1,Leaf);
    out.text(info.difficulty,x+panelWidth-15-info.difficulty.size()*6,28,1,Muted);
    const float namePixel=std::min(1.75f,386.0f/std::max(1.0f,info.name.size()*6.0f));
    out.text(info.name,x+17,45,namePixel,Cream);
    out.text("TIME "+crossingTime(game.crossingElapsed()),x+17,70,1,Cream);
    out.text("FALLS "+std::to_string(game.crossingFalls()),x+155,70,1,Muted);
    out.text(game.crossingCompleted() ? "EXIT REACHED"
                 : game.crossingCheckpointReached() ? "CHECKPOINT SAVED" : "CHECKPOINT AHEAD",
             x+271,70,1,game.crossingCompleted() || game.crossingCheckpointReached() ? Leaf : Muted);
    const auto best=game.crossingBestTime();
    out.text(best>0 ? "SESSION BEST "+crossingTime(best) : "CYAN CHECKPOINT / GOLD EXIT",
             x+17,86,0.75f,Muted);
    if (game.crossingCompleted()) {
        out.frame(center-170,113,340,62,Leaf);
        out.centered("CROSSING COMPLETE",center,126,1.75f,Leaf);
        out.centered("N - NEXT   SHIFT+R - RETRY   B - VILLAGE",center,153,1,Cream);
    } else if (game.status.starts_with("RETURNED") || game.status.starts_with("CHECKPOINT")
               || game.status.starts_with("NEW ATTEMPT")) {
        const auto feedback=std::string_view(game.status).substr(0,game.status.find(" | "));
        out.frame(center-170,109,340,24,Edge);
        out.centered(feedback.substr(0,52),center,117,1,Leaf);
    }
    out.frame(x,height-165,panelWidth,37,Edge);
    out.centered("N/P CHANGE  R CHECKPOINT  SHIFT+R RESTART",center,height-155,1,Cream);
    out.centered("F6 ALL COURSES  B VILLAGE  H CONTROLS",center,height-142,1,Muted);
}

class SavedState {
public:
    SavedState()
    {
        depthTest_ = glIsEnabled(GL_DEPTH_TEST);
        cullFace_ = glIsEnabled(GL_CULL_FACE);
        blend_ = glIsEnabled(GL_BLEND);
        scissorTest_ = glIsEnabled(GL_SCISSOR_TEST);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask_);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program_);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao_);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer_);
        glGetIntegerv(GL_POLYGON_MODE, polygonMode_.data());
        glGetIntegerv(GL_BLEND_SRC_RGB, &blendSourceRgb_);
        glGetIntegerv(GL_BLEND_DST_RGB, &blendDestinationRgb_);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSourceAlpha_);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDestinationAlpha_);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRgb_);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha_);
    }

    ~SavedState()
    {
        restore(GL_DEPTH_TEST, depthTest_);
        restore(GL_CULL_FACE, cullFace_);
        restore(GL_BLEND, blend_);
        restore(GL_SCISSOR_TEST, scissorTest_);
        glDepthMask(depthMask_);
        glBlendFuncSeparate(blendSourceRgb_, blendDestinationRgb_,
                            blendSourceAlpha_, blendDestinationAlpha_);
        glBlendEquationSeparate(blendEquationRgb_, blendEquationAlpha_);
        // Core OpenGL applies one polygon mode to both faces.
        glPolygonMode(GL_FRONT_AND_BACK, polygonMode_[0]);
        glUseProgram(static_cast<GLuint>(program_));
        glBindVertexArray(static_cast<GLuint>(vao_));
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(buffer_));
    }

private:
    static void restore(GLenum capability, GLboolean enabled)
    {
        if (enabled) glEnable(capability);
        else glDisable(capability);
    }

    GLboolean depthTest_ = GL_FALSE;
    GLboolean cullFace_ = GL_FALSE;
    GLboolean blend_ = GL_FALSE;
    GLboolean scissorTest_ = GL_FALSE;
    GLboolean depthMask_ = GL_TRUE;
    GLint program_ = 0;
    GLint vao_ = 0;
    GLint buffer_ = 0;
    std::array<GLint, 2> polygonMode_{};
    GLint blendSourceRgb_ = 0;
    GLint blendDestinationRgb_ = 0;
    GLint blendSourceAlpha_ = 0;
    GLint blendDestinationAlpha_ = 0;
    GLint blendEquationRgb_ = 0;
    GLint blendEquationAlpha_ = 0;
};

void render(const Drawing& drawing, const Shader& shader, GLuint vao, GLuint buffer)
{
    if (drawing.vertices.empty()) return;
    const SavedState savedState;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    shader.use();
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(drawing.vertices.size() * sizeof(Vertex)),
                 drawing.vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(drawing.vertices.size()));
}

} // namespace

Hud::Hud() : shader_(vertexShader, fragmentShader)
{
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &buffer_);
    if (!vao_ || !buffer_) {
        glDeleteBuffers(1, &buffer_);
        glDeleteVertexArrays(1, &vao_);
        throw std::runtime_error("Failed to allocate HUD resources");
    }
    GLint previousVao = 0;
    GLint previousBuffer = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousBuffer);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, color)));
    glBindVertexArray(static_cast<GLuint>(previousVao));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previousBuffer));
}

Hud::~Hud()
{
    glDeleteBuffers(1, &buffer_);
    glDeleteVertexArrays(1, &vao_);
}

void Hud::draw(std::string_view text, int framebufferWidth, int framebufferHeight,
               float contentScale)
{
    if (text.empty() || framebufferWidth <= 0 || framebufferHeight <= 0) return;
    if (!std::isfinite(contentScale) || contentScale <= 0.0f) contentScale = 1.0f;

    const float pixel = std::round(2.0f * std::clamp(contentScale, 0.5f, 4.0f));
    const float margin = 6.0f * pixel;
    const float padding = 4.0f * pixel;
    const float advance = 6.0f * pixel;
    const float lineHeight = 9.0f * pixel;
    const float availableWidth = framebufferWidth - 2.0f * (margin + padding);
    const float availableHeight = framebufferHeight - 2.0f * (margin + padding);
    if (availableWidth < advance || availableHeight < 7.0f * pixel) return;

    // Bound geometry even for unusually large framebuffers or input strings.
    const auto columns = static_cast<std::size_t>(std::min(160.0f, availableWidth / advance));
    const auto rows = static_cast<std::size_t>(std::min(32.0f,
        (availableHeight + 2.0f * pixel) / lineHeight));
    std::vector<std::string_view> lines;
    std::size_t widest = 0;
    while (!text.empty() && lines.size() < rows) {
        const std::size_t end = text.find('\n');
        const std::size_t length = end == std::string_view::npos ? text.size() : end;
        const auto line = text.substr(0, std::min(length, columns));
        widest = std::max(widest, line.size());
        lines.push_back(line);
        if (end == std::string_view::npos) break;
        text.remove_prefix(end + 1);
    }
    if (lines.empty()) return;

    Drawing drawing(static_cast<float>(framebufferWidth), static_cast<float>(framebufferHeight));
    drawing.vertices.reserve(6 + lines.size() * widest * 18 * 6);
    drawing.quad(margin, margin, widest * advance + 2.0f * padding,
                 lines.size() * lineHeight - 2.0f * pixel + 2.0f * padding, {0,0,0,0.75f});
    for (std::size_t line = 0; line < lines.size(); ++line)
        drawing.text(lines[line],margin+padding,margin+padding+line*lineHeight,pixel,glm::vec4(1));
    render(drawing, shader_, vao_, buffer_);
}

void Hud::drawPlay(const Game& game, int framebufferWidth, int framebufferHeight, float contentScale)
{
    if (game.editorMode() || framebufferWidth <= 0 || framebufferHeight <= 0) return;
    if (!std::isfinite(contentScale) || contentScale <= 0) contentScale = 1;
    // Preserve the layout on small windows, then apply the display's DPI scale.
    const float minWidth=game.crossingMenuOpen() ? 620.0f : 460.0f;
    const float minHeight=game.crossingMenuOpen() ? 470.0f : 440.0f;
    const float scale = std::min({std::clamp(contentScale,0.5f,4.0f),
                                 framebufferWidth / minWidth, framebufferHeight / minHeight});
    const float width = framebufferWidth / scale, height = framebufferHeight / scale;
    Drawing drawing(width,height);
    drawing.vertices.reserve(24000);
    const auto& equipment = game.equipment;
    const auto selected = equipment.selectedTool();

    const auto offset = game.compassTarget() - game.camera.position();
    const glm::vec2 target(offset.x,offset.z);
    const auto forward3 = game.camera.forward();
    glm::vec2 forward(forward3.x,forward3.z);
    if (glm::length(forward) > 0.0001f) forward = glm::normalize(forward);
    else forward = {0,-1};
    const glm::vec2 right(-forward.y,forward.x);
    glm::vec2 needle(glm::dot(target,right),-glm::dot(target,forward));
    if (glm::length(needle) > 0.1f) needle = glm::normalize(needle);
    else needle = {0,-1};
    const float center = width * 0.5f;
    const float barTop = height - 80;
    constexpr float slotWidth = 58, gap = 7;
    const float barLeft = center - (3 * slotWidth + 2 * gap) * 0.5f;
    drawing.quad(barLeft-7,barTop-7,202,72,{0.01f,0.03f,0.04f,0.68f});
    for (int slot=0; slot<ToolCount; ++slot) {
        const float x = barLeft + slot * (slotWidth+gap);
        const bool active = slot == equipment.selectedSlot;
        drawing.frame(x,barTop,slotWidth,58,active ? Gold : Edge,
                      active ? glm::vec4(0.10f,0.15f,0.14f,0.96f) : Panel);
        drawing.quad(x+5,barTop+5,10,11,{0.02f,0.04f,0.05f,1});
        drawing.text(std::to_string(slot+1),x+8,barTop+7,1,active ? Gold : Muted);
        const glm::vec2 icon(x+29,barTop+31);
        if (slot == 0) pickaxeIcon(drawing,icon,0.95f,equipment.pickaxeTier);
        else if (slot == 1) lanternIcon(drawing,icon,0.95f);
        else compassIcon(drawing,icon,0.95f,needle);
        if (slot == 0) {
            drawing.quad(x+35,barTop+43,19,11,{0.08f,0.12f,0.12f,1});
            drawing.text("T"+std::to_string(equipment.pickaxeTier),x+39,barTop+45,1,Gold);
        }
        if (active) drawing.quad(x+6,barTop+54,slotWidth-12,2,Gold);
    }
    std::string title(toolName(selected));
    std::string hint;
    if (selected == Tool::Pickaxe) {
        title += " - TIER " + std::to_string(equipment.pickaxeTier);
        hint = game.rootCrossingActive() ? "MINING COMING LATER" : "UPGRADE AT THE BLACKSMITH";
    } else if (selected == Tool::Lantern) {
        hint = "WARM LIGHT WHILE EQUIPPED";
    } else {
        const int distance = static_cast<int>(std::round(glm::length(target)));
        const int bearing = static_cast<int>(std::round(glm::degrees(std::atan2(needle.x,-needle.y))));
        hint = std::string(game.rootCrossingActive() ? "EXIT " : "HEART TREE ")
            + std::to_string(distance) + "M";
        if (distance > 1)
            hint += "  " + std::to_string(std::abs(bearing)) + " DEG " + (bearing < 0 ? "LEFT" : "RIGHT");
    }
    const float labelWidth = std::max(title.size()*9.0f,hint.size()*6.0f)+18;
    drawing.quad(center-labelWidth*0.5f,barTop-38,labelWidth,33,{0.015f,0.035f,0.045f,0.82f});
    drawing.centered(title,center,barTop-33,1.5f,Cream);
    drawing.centered(hint,center,barTop-16,1,Muted);

    if (game.rootCrossingActive() && !game.crossingMenuOpen())
        crossingProgress(drawing,game,width,height);

    if (game.nearBlacksmith() && !game.menuOpen()) {
        constexpr float promptWidth = 160;
        drawing.frame(center-promptWidth*0.5f,height*0.5f+35,promptWidth,25,Edge);
        drawing.centered("E - BLACKSMITH",center,height*0.5f+43,1.25f,Gold);
    }
    if (game.nearExpeditionGate() && !game.menuOpen()) {
        constexpr float promptWidth=234;
        drawing.frame(center-promptWidth*0.5f,height*0.5f+35,promptWidth,25,Leaf);
        drawing.centered("E - ROOT CROSSING TESTS",center,height*0.5f+43,1.25f,Leaf);
    }
    if (game.smithOpen()) {
        drawing.quad(0,0,width,height,{0.01f,0.025f,0.035f,0.55f});
        constexpr float panelWidth = 410, panelHeight = 300;
        const float x = center-panelWidth*0.5f, y = height*0.5f-panelHeight*0.5f-28;
        drawing.quad(x+5,y+6,panelWidth,panelHeight,{0,0,0,0.35f});
        drawing.frame(x,y,panelWidth,panelHeight,Edge);
        drawing.quad(x+2,y+2,panelWidth-4,3,Gold);
        drawing.text("BLACKSMITH",x+22,y+19,2,Gold);
        drawing.text("THE VILLAGE FORGE",x+23,y+42,1,Muted);
        drawing.quad(x+20,y+61,panelWidth-40,1,Edge);
        drawing.frame(x+22,y+77,80,74,Edge,{0.075f,0.13f,0.14f,1});
        pickaxeIcon(drawing,{x+61,y+115},1.75f,equipment.pickaxeTier);
        drawing.text("PICKAXE",x+122,y+81,1.75f,Cream);
        drawing.text("CURRENT TIER "+std::to_string(equipment.pickaxeTier),x+122,y+103,1.25f,Muted);
        const auto cost = pickaxeUpgradeCost(equipment.pickaxeTier);
        const bool affordable = cost && equipment.copper >= cost->copper && equipment.iron >= cost->iron;
        drawing.text(cost ? "NEXT TIER "+std::to_string(equipment.pickaxeTier+1) : "FULLY UPGRADED",
                     x+122,y+124,1.25f,Gold);
        std::string feedback = game.status;
        std::replace(feedback.begin(),feedback.end(),'\n',' ');
        std::replace(feedback.begin(),feedback.end(),'\r',' ');
        if (feedback.size() > 60) feedback = feedback.substr(0,57) + "...";
        const bool error = feedback.starts_with("Upgrade not saved") || feedback.starts_with("Progress unavailable")
            || feedback.starts_with("Not enough");
        drawing.text(feedback,x+24,y+153,1,error ? glm::vec4(0.94f,0.53f,0.39f,1)
                     : feedback.starts_with("Pickaxe upgraded") ? Gold : Muted);
        drawing.text("UPGRADE COST",x+24,y+166,1,Muted);
        drawing.text("IN YOUR PACK",x+225,y+166,1,Muted);
        drawing.text(cost ? std::to_string(cost->copper)+" COPPER" : "-",x+24,y+183,1.25f,
                     cost && equipment.copper < cost->copper ? glm::vec4(0.91f,0.47f,0.35f,1) : Cream);
        drawing.text(cost ? std::to_string(cost->iron)+" IRON" : "-",x+24,y+202,1.25f,
                     cost && equipment.iron < cost->iron ? glm::vec4(0.91f,0.47f,0.35f,1) : Cream);
        drawing.text(std::to_string(equipment.copper)+" COPPER",x+225,y+183,1.25f,Cream);
        drawing.text(std::to_string(equipment.iron)+" IRON",x+225,y+202,1.25f,Cream);
        drawing.frame(x+22,y+226,panelWidth-44,30,affordable ? Gold : Edge,
                      affordable ? glm::vec4(0.77f,0.51f,0.18f,1) : glm::vec4(0.09f,0.14f,0.15f,1));
        drawing.centered(!cost ? "MAXIMUM TIER" : affordable ? "ENTER - UPGRADE" : "MORE MATERIALS NEEDED",
                         center,y+236,1.5f,affordable ? Panel : Muted);
        drawing.centered("E / ESC - CLOSE",center,y+268,1, Cream);
        drawing.centered("STARTER GIFT: 12 COPPER",center,y+283,1,Muted);
    }
    if (game.crossingMenuOpen()) crossingPicker(drawing,game,width,height);
    render(drawing, shader_, vao_, buffer_);
}

} // namespace voxel
