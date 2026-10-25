#include "graphics/hud.h"
#include "game.h"
#include "equipment.h"
#include "root_crossings.h"
#include "section_editor.h"
#include "level_editor.h"

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
layout(location = 2) in vec2 uv;
out vec4 pixelColor;
out vec2 pixelUv;
void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    pixelColor = color;
    pixelUv = uv;
}
)glsl";

constexpr const char* fragmentShader = R"glsl(#version 410 core
in vec4 pixelColor;
out vec4 fragmentColor;
void main() { fragmentColor = pixelColor; }
)glsl";

constexpr const char* previewFragmentShader = R"glsl(#version 410 core
in vec4 pixelColor;
in vec2 pixelUv;
uniform sampler2D previewAtlas;
out vec4 fragmentColor;
void main() {
    fragmentColor = pixelColor;
    if (pixelUv.x >= 0.0) {
        vec4 preview = texture(previewAtlas, pixelUv);
        // Atlas coverage is premultiplied by transparent-border filtering.
        // The HUD uses straight alpha, so keep antialiased edges free of halos.
        if (preview.a > 0.0) preview.rgb /= preview.a;
        fragmentColor *= preview;
    }
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
    glm::vec2 uv{-1, -1};
};

// The field satchel palette is shared by play, authoring menus and the pack.
constexpr glm::vec4 Cream{0.95f,0.91f,0.79f,1};
constexpr glm::vec4 Muted{0.73f,0.64f,0.49f,1};
constexpr glm::vec4 Gold{0.95f,0.70f,0.30f,1};
constexpr glm::vec4 Panel{0.09f,0.061f,0.043f,0.98f};
constexpr glm::vec4 Edge{0.46f,0.34f,0.22f,1};
constexpr glm::vec4 Canvas{0.16f,0.105f,0.072f,1};
constexpr glm::vec4 Selected{0.24f,0.18f,0.10f,1};
constexpr glm::vec4 Stitch{0.54f,0.42f,0.27f,0.70f};
constexpr glm::vec4 Leaf{0.45f,0.86f,0.60f,1};

// Small colored triangles and bitmap glyphs share one buffer and draw call.
// Coordinates start at the top-left; play UI uses logical pixels for high DPI.
class Drawing {
public:
    Drawing(float width, float height, float physicalScale=0)
        : width_(width), height_(height), physicalScale_(physicalScale) {}
    float physicalScale() const { return physicalScale_; }
    float fontPixel(float pixel) const {
        return physicalScale_>0 ? std::max(1.0f,std::round(pixel*physicalScale_))/physicalScale_ : pixel;
    }
    std::vector<Vertex> vertices;
    std::vector<int> previews;

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
    void gradientQuad(const std::array<glm::vec2,4>& points,
                      const std::array<glm::vec4,4>& colors)
    {
        for (const int corner : {0,1,2,0,2,3}) {
            const auto p=points[corner];
            vertices.push_back({2*p.x/width_-1,1-2*p.y/height_,colors[corner]});
        }
    }
    void preview(int id, glm::vec2 center, float size)
    {
        previews.push_back(id);
        const auto uv = PalettePreviews::uvBounds(id);
        const auto low = center - size * .5f, high = center + size * .5f;
        const std::array<glm::vec2,4> points{{low,{high.x,low.y},high,{low.x,high.y}}};
        const std::array<glm::vec2,4> texels{{{uv.x,uv.w},{uv.z,uv.w},{uv.z,uv.y},{uv.x,uv.y}}};
        for (const int corner : {0,1,2,0,2,3}) {
            const auto p = points[corner];
            vertices.push_back({2*p.x/width_-1,1-2*p.y/height_,glm::vec4(1),texels[corner]});
        }
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
        pixel=fontPixel(pixel);
        if (physicalScale_>0) {
            x=std::round(x*physicalScale_)/physicalScale_;
            y=std::round(y*physicalScale_)/physicalScale_;
        }
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
        pixel=fontPixel(pixel);
        text(value, center - (value.size() * 6 - (value.empty() ? 0 : 1)) * pixel * 0.5f, y, pixel, color);
    }
    void frame(float x, float y, float width, float height, glm::vec4 border,
               glm::vec4 fill = Panel, float corner = 6)
    {
        if (width<=4 || height<=4) return;
        corner=std::min({corner,width*.2f,height*.2f});
        const auto polygon=[&](float left,float top,float w,float h,float cut,glm::vec4 color) {
            const std::array<glm::vec2,8> points{{{left+cut,top},{left+w-cut,top},
                {left+w,top+cut},{left+w,top+h-cut},{left+w-cut,top+h},
                {left+cut,top+h},{left,top+h-cut},{left,top+cut}}};
            const glm::vec2 center(left+w*.5f,top+h*.5f);
            for (std::size_t i=0;i<points.size();++i)
                triangle(center,points[i],points[(i+1)%points.size()],color);
        };
        polygon(x,y,width,height,corner,border);
        polygon(x+2,y+2,width-4,height-4,std::max(1.0f,corner-1),fill);
        // Keep seams away from labels on small buttons and counts in hotbar pockets.
        if (width>=34 && height>=36 && fill.a>.9f) {
            const bool pocket=width<=80;
            for (float step=pocket ? 25 : 10;step<width-10;step+=9)
                quad(x+step,y+5,4,1,Stitch);
            for (float step=10;step<width-(pocket ? 25 : 10);step+=9)
                quad(x+step,y+height-6,4,1,Stitch);
            for (float step=pocket ? 22 : 12;step<height-10;step+=9) {
                quad(x+5,y+step,1,4,Stitch);
                quad(x+width-6,y+step,1,4,Stitch);
            }
        }
    }

private:
    float width_, height_, physicalScale_;
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

// The same bevelled blade, brass guard and wrapped grip appear in the pocket
// and in hand. Keeping its silhouette consistent makes slot four recognizable.
void swordIcon(Drawing& out,glm::vec2 center,float scale,float angle=0)
{
    const float c=std::cos(angle),s=std::sin(angle);
    const auto p=[&](float x,float y) {return center+glm::vec2(c*x-s*y,s*x+c*y)*scale;};
    const auto face=[&](glm::vec2 a,glm::vec2 b,glm::vec2 c,glm::vec2 d,glm::vec4 color) {
        out.triangle(p(a.x,a.y),p(b.x,b.y),p(c.x,c.y),color);
        out.triangle(p(a.x,a.y),p(c.x,c.y),p(d.x,d.y),color);
    };
    constexpr glm::vec4 steel{.58f,.72f,.77f,1},shadow{.25f,.37f,.43f,1};
    constexpr glm::vec4 edge{.87f,.95f,.92f,1},brass{.73f,.44f,.17f,1};
    face({-6,1},{-6,-39},{0,-50},{0,1},steel);
    face({0,1},{0,-50},{6,-39},{6,1},shadow);
    face({-6,-39},{0,-50},{-3,-38},{-3,1},edge);
    out.triangle(p(-6,-39),p(-3,1),p(-6,1),edge);
    face({0,-45},{1,-38},{1,-2},{0,-2},{.72f,.87f,.88f,1});
    face({-13,0},{-5,-2},{5,-2},{13,0},Gold);
    face({-13,0},{13,0},{12,5},{-12,5},brass);
    face({-4,5},{4,5},{4,23},{-4,23},{.23f,.13f,.09f,1});
    for(int wrap=0;wrap<4;++wrap)
        out.bar(p(-3,7+wrap*4),p(3,9+wrap*4),1.2f*scale,{.48f,.29f,.15f,1});
    face({-5,23},{5,23},{4,28},{-4,28},brass);
    out.triangle(p(-4,23),p(4,23),p(0,26),Gold);
    out.triangle(p(0,-1),p(3,2),p(0,5),{.18f,.72f,.70f,1});
    out.triangle(p(0,-1),p(0,5),p(-3,2),{.51f,.92f,.81f,1});
}

void toolBelt(Drawing& out,const Equipment& equipment,float center,float top,glm::vec2 needle={0,-1})
{
    constexpr float slotWidth=48,gap=6,total=ToolCount*slotWidth+(ToolCount-1)*gap;
    const float left=center-total*.5f;
    for(int slot=0;slot<ToolCount;++slot) {
        const float x=left+slot*(slotWidth+gap);
        const bool active=slot==equipment.selectedSlot;
        out.frame(x,top,slotWidth,48,active ? Gold : glm::vec4(.30f,.25f,.19f,.65f),
            active ? glm::vec4(.20f,.14f,.08f,.86f) : glm::vec4(.055f,.045f,.035f,.68f),3);
        out.text(std::to_string(slot+1),x+5,top+5,.8f,active ? Gold : Muted);
        const glm::vec2 icon(x+24,top+26);
        switch(static_cast<Tool>(slot)) {
        case Tool::Pickaxe: pickaxeIcon(out,icon,.75f,equipment.pickaxeTier); break;
        case Tool::Lantern: lanternIcon(out,icon,.75f); break;
        case Tool::Compass: compassIcon(out,icon,.75f,needle); break;
        case Tool::Sword: swordIcon(out,icon+glm::vec2(-1,4),.39f,.40f); break;
        }
        if(slot==0) {
            for (int tier=0;tier<3;++tier)
                out.quad(x+31+tier*4,top+39,2,3,tier<equipment.pickaxeTier ? Gold : Edge);
        }
        if(active) out.quad(x+5,top+45,slotWidth-10,2,Gold);
    }
}

std::string crossingTime(float seconds)
{
    const int tenths = std::isfinite(seconds)
        ? static_cast<int>(std::clamp(seconds,0.0f,5999.9f)*10.0f) : 0;
    const int minutes = tenths/600, remainder = tenths/10%60;
    return std::to_string(minutes)+":"+(remainder<10 ? "0" : "")
        +std::to_string(remainder)+"."+std::to_string(tenths%10);
}

void fittedText(Drawing& out, std::string_view text, float x, float y, float width,
                float pixel=1.0f, glm::vec4 color=Cream)
{
    if (text.empty()) return;
    pixel=out.fontPixel(std::min(pixel,width/(text.size()*6.0f-1)));
    const auto columns=static_cast<std::size_t>(std::max(1.0f,(width/pixel+1)/6));
    if (text.size()>columns) {
        const auto clipped=std::string(text.substr(0,columns>3 ? columns-3 : columns))+(columns>3 ? "..." : "");
        out.text(clipped,x,y,pixel,color);
    } else out.text(text,x,y,pixel,color);
}

void fittedCentered(Drawing& out,std::string_view text,float center,float y,float width,
                    float pixel=1,glm::vec4 color=Cream)
{
    if (text.empty()) return;
    pixel=out.fontPixel(std::min(pixel,width/(text.size()*6.0f-1)));
    const auto columns=static_cast<std::size_t>(std::max(1.0f,(width/pixel+1)/6));
    if (text.size()>columns) {
        const auto clipped=std::string(text.substr(0,columns>3 ? columns-3 : columns))+(columns>3 ? "..." : "");
        out.centered(clipped,center,y,pixel,color);
    } else out.centered(text,center,y,pixel,color);
}

// Gameplay labels use a shadow instead of the framed panels used in menus.
void quietText(Drawing& out, std::string_view text, float x, float y, float width,
               glm::vec4 color=Cream)
{
    fittedText(out,text,x+1,y+1,width,1,{0,0,0,.9f});
    fittedText(out,text,x,y,width,1,color);
}

void quietCentered(Drawing& out, std::string_view text, float center, float y, float width,
                   glm::vec4 color=Cream)
{
    fittedCentered(out,text,center+1,y+1,width,1,{0,0,0,.9f});
    fittedCentered(out,text,center,y,width,1,color);
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
    out.quad(0,0,width,height,{.025f,.016f,.009f,.72f});
    constexpr float panelWidth=578, panelHeight=420;
    const float x=(width-panelWidth)*0.5f, y=(height-panelHeight)*0.5f;
    out.quad(x+6,y+7,panelWidth,panelHeight,{0,0,0,0.35f});
    out.frame(x,y,panelWidth,panelHeight,Edge);
    out.quad(x+12,y+12,panelWidth-24,43,Canvas);
    out.text("ROOT CROSSINGS",x+23,y+18,2,Cream);
    fittedText(out,"LUSH CAVES / 10 PLAYABLE LAYOUTS",x+24,y+43,panelWidth-48,1,Muted);
    fittedText(out,"ESC - CLOSE",x+panelWidth-112,y+22,90,1,Muted);

    const auto catalog=rootCrossingCatalog();
    const int selection=std::clamp(game.crossingMenuSelection(),0,RootCrossingCount-1);
    constexpr float cardWidth=260, cardHeight=43;
    for (int index=0; index<RootCrossingCount; ++index) {
        const auto& info=catalog[index];
        const bool selected=index==selection;
        const float cardX=x+23+(index/5)*(cardWidth+12);
        const float cardY=y+66+(index%5)*49;
        out.frame(cardX,cardY,cardWidth,cardHeight,selected ? Gold : Edge,
                  selected ? Selected : Panel);
        out.quad(cardX+7,cardY+8,18,25,selected ? Gold : Canvas);
        out.centered(std::to_string((index+1)%10),cardX+16,cardY+16,1.25f,
                     selected ? Panel : Muted);
        fittedText(out,info.name,cardX+33,cardY+9,213,1.25f,selected ? Cream : Muted);
        fittedText(out,info.difficulty,cardX+33,cardY+27,213,1,selected ? Leaf : Muted);
    }
    out.quad(x+23,y+316,panelWidth-46,1,Edge);
    wrappedDescription(out,catalog[selection].description,x+24,y+330,
                       static_cast<std::size_t>((panelWidth-48)/(6*out.fontPixel(1))));
    fittedText(out,"CYAN CHECKPOINT / GOLD EXIT / FALLS RESET",x+24,y+359,panelWidth-48,0.9f,Leaf);
    fittedText(out,"UP/DOWN OR 1-9/0 - SELECT",x+24,y+376,322,1,Muted);
    fittedText(out,"WASD MOVE / SPACE JUMP / SHIFT SPRINT",x+24,y+395,322,1,Muted);
    out.frame(x+360,y+370,194,34,Gold,{0.72f,0.49f,0.20f,1});
    fittedCentered(out,"ENTER - START",x+457,y+382,174,1.5f,Panel);
}

void crossingProgress(Drawing& out, const Game& game, float width, float height)
{
    const int index=game.rootCrossingIndex();
    if (index<0 || index>=RootCrossingCount) return;
    const auto& info=rootCrossingCatalog()[index];
    quietText(out,info.name,18,20,width-36);
    const auto progress=game.crossingCompleted() ? "COMPLETE / N NEXT / SHIFT+R RETRY"
        : game.crossingCheckpointReached() ? "CHECKPOINT SAVED" : "ROOT CROSSING";
    quietText(out,progress,18,36,width-36,game.crossingCompleted() || game.crossingCheckpointReached() ? Leaf : Muted);
    if (game.crossingCompleted())
        quietCentered(out,"FINISHED IN "+crossingTime(game.crossingElapsed()),width*.5f,height*.35f,360,Leaf);
}

struct SectionHit {
    float x, y, width, height;
    int action;
};

struct SectionCanvas {
    float scale, width, height;
};

SectionCanvas sectionCanvas(int width, int height, float contentScale)
{
    if (!std::isfinite(contentScale) || contentScale <= 0) contentScale = 1;
    const float scale=std::min({std::clamp(contentScale,0.5f,4.0f),width/736.0f,height/516.0f});
    return {scale,width/scale,height/scale};
}


void sectionButton(Drawing& out, std::vector<SectionHit>& hits, float x,float y,float width,float height,
                   std::string_view text,int action,bool active=false,bool enabled=true,float textPixel=1.1f)
{
    const auto color=enabled ? (active ? Gold : Edge) : glm::vec4(.23f,.18f,.13f,1);
    out.frame(x,y,width,height,color,active ? Selected : Panel);
    const float pixel=out.fontPixel(std::min(textPixel,(width-14)/std::max(1.0f,text.size()*6.0f)));
    const auto columns=static_cast<std::size_t>(std::max(1.0f,((width-12)/pixel+1)/6));
    std::string clipped(text.substr(0,columns));
    if (text.size()>columns && columns>3) clipped=std::string(text.substr(0,columns-3))+"...";
    out.centered(clipped,x+width*0.5f,y+(height-7*pixel)*0.5f,pixel,enabled ? (active ? Gold : Cream) : Muted);
    if (enabled) hits.push_back({x,y,width,height,action});
}

void sectionPanel(Drawing& out,float width,float height,float x,float y,float w,float h,
                  std::string_view title,std::string_view subtitle)
{
    out.quad(0,0,width,height,{.025f,.016f,.009f,.76f});
    out.quad(x+6,y+7,w,h,{0,0,0,0.35f});
    out.frame(x,y,w,h,Edge);
    out.quad(x+12,y+12,w-24,43,Canvas);
    fittedText(out,title,x+22,y+18,w-44,2,Cream);
    fittedText(out,subtitle,x+23,y+43,w-46,1,Muted);
}

void oreIcon(Drawing& out, ItemId item, glm::vec2 center, float scale);

void sectionItemIcon(Drawing& out,const SectionPaletteItem& item,glm::vec2 center,float size)
{
    if (item.tool) {
        const auto p=[&](float x,float y){return center+glm::vec2(x,y)*size;};
        if (*item.tool==SectionBuildMode::CarveRound || *item.tool==SectionBuildMode::CarveBox) {
            // The cutout shows the brush shape, with a pick to indicate removal.
            if (*item.tool==SectionBuildMode::CarveRound) {
                for (int side=0;side<8;++side) {
                    const float a=side*glm::pi<float>()/4, b=(side+1)*glm::pi<float>()/4;
                    out.bar(p(std::cos(a)*.35f,std::sin(a)*.35f),
                            p(std::cos(b)*.35f,std::sin(b)*.35f),size*.045f,Leaf);
                }
            } else out.frame(center.x-size*.35f,center.y-size*.35f,size*.7f,size*.7f,Leaf,Canvas);
            pickaxeIcon(out,center,size/48.f,1);
        } else if (*item.tool==SectionBuildMode::Select) {
            for (float sx:{-1.f,1.f}) for (float sy:{-1.f,1.f}) {
                out.bar(p(sx*.35f,sy*.16f),p(sx*.35f,sy*.35f),size*.06f,Gold);
                out.bar(p(sx*.16f,sy*.35f),p(sx*.35f,sy*.35f),size*.06f,Gold);
            }
        } else out.preview(PalettePreviews::blockId(Block::Stone),center,size);
    } else if (item.pickup) {
        out.preview(PalettePreviews::itemId(*item.pickup),center,size);
    } else if (item.asset) {
        out.preview(PalettePreviews::assetId(*item.asset),center,size);
    } else if (item.block) {
        out.preview(PalettePreviews::blockId(*item.block),center,size);
    } else if (item.marker!=SectionMarker::None) {
        const auto color=item.marker==SectionMarker::Start ? Leaf
            : item.marker==SectionMarker::Checkpoint ? glm::vec4(0.28f,0.87f,0.93f,1) : Gold;
        const auto p=[&](float x,float y){return center+glm::vec2(x,y)*size;};
        out.bar(p(-0.15f,0.43f),p(-0.15f,-0.43f),size*0.07f,Cream);
        out.triangle(p(-0.1f,-0.40f),p(0.38f,-0.23f),p(-0.1f,-0.05f),color);
        out.bar(p(-0.34f,0.43f),p(0.12f,0.43f),size*0.08f,Edge);
    }
}

std::string_view sectionItemGroup(const SectionPaletteItem& item)
{
    if (item.pickup) return "DROPPED ITEM";
    if (item.asset) for (const auto& entry:sectionAssetCatalog()) if (entry.kind==*item.asset) return entry.group;
    if (item.block) return "SOLID BUILDING BLOCK";
    return "SECTION MARKER";
}

void sectionHotbar(Drawing& out,std::vector<SectionHit>& hits,const SectionEditor& editor,
                   float center,float y,bool clickable)
{
    constexpr float slot=48,gap=5;
    const float x=center-(9*slot+8*gap)*0.5f;
    for (int index=0;index<9;++index) {
        const float left=x+index*(slot+gap);
        const bool selected=index==editor.selectedSlot();
        out.frame(left,y,slot,slot,selected ? Gold : Edge,selected ? Selected : Panel);
        sectionItemIcon(out,editor.hotbar()[index],{left+24,y+26},36);
        out.quad(left+4,y+4,9,10,Panel);
        out.text(std::to_string(index+1),left+6,y+5,1,selected ? Gold : Muted);
        if (selected && editor.hotbar()[index].pickup) {
            const auto quantity=std::to_string(editor.pickupQuantity());
            out.quad(left+23,y+30,21,12,Panel);
            out.text(quantity,left+43-(quantity.size()*6-1)*out.fontPixel(1),y+32,1,Gold);
        }
        if (selected) out.quad(left+5,y+slot-5,slot-10,2,Gold);
        if (clickable) hits.push_back({left,y,slot,slot,SectionUiHotbarBase+index});
    }
}

std::string sectionWorldName(SectionWorld world)
{
    for (const auto& item:sectionWorldCatalog()) if (item.id==world) return std::string(item.name);
    return "Unknown world";
}

std::string sectionCategoryName(SectionCategory category)
{
    for (const auto& item:sectionCategoryCatalog()) if (item.id==category) return std::string(item.name);
    return "Unsorted";
}

void sectionFeedback(Drawing& out,const SectionEditor& editor,float x,float y,float width)
{
    auto feedback=editor.message();
    std::replace(feedback.begin(),feedback.end(),'\n',' ');
    if (feedback.size()>96) feedback=feedback.substr(0,93)+"...";
    fittedText(out,feedback,x,y,width,1,Leaf);
}

void sectionWorldMenu(Drawing& out,std::vector<SectionHit>& hits,const SectionEditor& editor,float width,float height)
{
    constexpr float w=620,h=410;
    const float x=(width-w)*0.5f,y=(height-h)*0.5f;
    sectionPanel(out,width,height,x,y,w,h,"SECTION WORKSHOP","CHOOSE THE WORLD YOUR NEW SECTION WILL BELONG TO");
    const auto worlds=sectionWorldCatalog();
    for (int index=0;index<static_cast<int>(worlds.size());++index) {
        const auto& world=worlds[index];
        const float top=y+72+index*78;
        const auto color=index==0 ? Leaf : index==1 ? glm::vec4(0.36f,0.79f,0.94f,1) : glm::vec4(0.98f,0.53f,0.25f,1);
        const bool selected=index==editor.selection();
        out.frame(x+22,top,w-44,68,selected ? Gold : Edge,selected ? Selected : Panel);
        out.quad(x+30,top+10,4,48,color);
        out.text("0"+std::to_string(index+1),x+48,top+24,2,color);
        fittedText(out,world.name,x+94,top+13,w-130,1.8f,Cream);
        fittedText(out,world.description,x+95,top+42,w-133,1,Muted);
        hits.push_back({x+22,top,w-44,68,SectionUiWorldBase+index});
    }
    out.text("BUILD / WALK YOUR ROUTE / NAME AND ACCEPT",x+24,y+319,1,Leaf);
    out.text("64 X 48 X 64 BLOCK BUILDING ROOM",x+24,y+339,1,Muted);
    sectionButton(out,hits,x+22,y+365,158,28,"B - VILLAGE",SectionUiBack);
    sectionButton(out,hits,x+192,y+365,176,28,"F11 LEVEL CREATOR",SectionUiLevelCreator);
    sectionButton(out,hits,x+w-242,y+365,220,28,"ENTER - CHOOSE WORLD",SectionUiConfirm,true);
}

void sectionLibraryMenu(Drawing& out,std::vector<SectionHit>& hits,const SectionEditor& editor,float width,float height)
{
    constexpr float w=700,h=470;
    const float x=(width-w)*0.5f,y=(height-h)*0.5f;
    sectionPanel(out,width,height,x,y,w,h,"YOUR SECTIONS",sectionWorldName(editor.selectedWorld())+" / DRAFTS AND ACCEPTED SECTIONS");
    sectionButton(out,hits,x+22,y+65,318,37,"+ OPEN ROOM / PARKOUR",SectionUiLibraryNew,editor.selection()==0);
    sectionButton(out,hits,x+360,y+65,318,37,"+ CAVE / CARVE FROM STONE",SectionUiLibraryCave);
    const auto& drafts=editor.draftEntries();
    const auto& entries=editor.entries();
    const int count=static_cast<int>(drafts.size()+entries.size());
    const int page=std::max(0,editor.selection()-1)/6;
    for (int row=0;row<6;++row) {
        const int index=page*6+row;
        const float top=y+111+row*40;
        if (index>=count) continue;
        const bool draft=index<static_cast<int>(drafts.size());
        const auto& entry=draft ? drafts[index] : entries[index-static_cast<int>(drafts.size())];
        const bool selected=editor.selection()==index+1;
        out.frame(x+22,top,w-44,35,selected ? Gold : Edge,selected ? Selected : Panel);
        fittedText(out,entry.meta.name,x+35,top+8,400,1.15f,Cream);
        out.text(draft ? "DRAFT / CONTINUE" : "ACCEPTED / EDIT COPY",x+487,top+9,0.9f,draft ? Gold : Leaf);
        fittedText(out,sectionCategoryName(entry.meta.category),x+35,top+23,390,0.8f,Muted);
        hits.push_back({x+22,top,w-44,35,draft ? SectionUiLibraryDraftBase+index
                        : SectionUiLibraryEntryBase+index-static_cast<int>(drafts.size())});
    }
    if (!count) {
        out.centered("YOUR FIRST SECTION STARTS HERE",width*0.5f,y+190,1.5f,Cream);
        out.centered("BUILD A ROUTE, PLAYTEST IT, THEN ACCEPT IT.",width*0.5f,y+218,1,Muted);
        out.centered("ACCEPTED SECTIONS APPEAR IN THIS WORLD'S LIBRARY.",width*0.5f,y+240,1,Muted);
    }
    sectionButton(out,hits,x+22,y+358,100,26,"< PREVIOUS",SectionUiLibraryPrevious,false,page>0);
    out.centered(std::to_string(count)+" SAVED / PAGE "+std::to_string(page+1)+" OF "+std::to_string(std::max(1,(count+5)/6)),width*0.5f,y+367,1,Muted);
    sectionButton(out,hits,x+w-122,y+358,100,26,"NEXT >",SectionUiLibraryNext,false,(page+1)*6<count);
    sectionFeedback(out,editor,x+24,y+396,w-48);
    sectionButton(out,hits,x+22,y+h-46,138,29,"ESC - BACK",SectionUiBack);
    sectionButton(out,hits,x+172,y+h-46,256,29,"F11 - LEVEL CREATOR",SectionUiLevelCreator);
    sectionButton(out,hits,x+w-230,y+h-46,208,29,"ENTER - OPEN",SectionUiConfirm,true);
}

void sectionPaletteMenu(Drawing& out,std::vector<SectionHit>& hits,const SectionEditor& editor,float width,float height)
{
    constexpr float w=700,h=478;
    const float x=(width-w)*0.5f,y=(height-h)*0.5f;
    sectionPanel(out,width,height,x,y,w,h,"CREATIVE INVENTORY",editor.paletteTab()==4
        ? "CHOOSE A BUILD TOOL / G CHANGES BRUSH SIZE AND LIGHTING"
        : "CHOOSE A SLOT BELOW, THEN CLICK AN ITEM TO EQUIP IT");
    const auto catalog=sectionAssetCatalog();
    const auto modelCount=std::count_if(catalog.begin(),catalog.end(),[](const auto& entry){return isCreativeAsset(entry.kind);});
    const std::array<std::string,5> tabs{"BLOCKS "+std::to_string(PlaceableBlocks.size()),
        "ASSETS "+std::to_string(modelCount),"ITEMS "+std::to_string(ItemCount),"MARKERS","TOOLS"};
    constexpr std::array tabIds{0,1,3,2,4};
    for (int tab=0;tab<static_cast<int>(tabs.size());++tab)
        sectionButton(out,hits,x+22+tab*110,y+62,104,26,tabs[tab],SectionUiPaletteTabBase+tabIds[tab],editor.paletteTab()==tabIds[tab]);
    sectionButton(out,hits,x+584,y+62,94,26,"ESC CLOSE",SectionUiBack);
    const auto& items=editor.paletteItems();
    const int selection=std::clamp(editor.paletteSelection(),0,std::max(0,static_cast<int>(items.size())-1));
    const int page=selection/24;
    constexpr float cellWidth=104,cellHeight=54,gap=6;
    for (int cell=0;cell<24;++cell) {
        const int index=page*24+cell;
        const float left=x+22+(cell%6)*(cellWidth+gap),top=y+97+(cell/6)*(cellHeight+gap);
        if (index>=static_cast<int>(items.size())) continue;
        const auto& item=items[index];
        const bool selected=index==selection;
        out.frame(left,top,cellWidth,cellHeight,selected ? Gold : Edge,selected ? Selected : Panel);
        const bool compact=out.physicalScale()<0.9f;
        sectionItemIcon(out,item,compact ? glm::vec2(left+52,top+14) : glm::vec2(left+24,top+27),compact ? 25 : 40);
        const float nameX=left+(compact ? 5 : 45),nameWidth=cellWidth-(compact ? 10 : 49);
        std::string_view name=item.name;
        auto split=name.size()>9 ? name.rfind(' ',10) : std::string_view::npos;
        if (split==std::string_view::npos && name.size()>9) split=std::min<std::size_t>(9,name.size());
        if (split!=std::string_view::npos) {
            fittedText(out,name.substr(0,split),nameX,top+(compact ? 28 : 17),nameWidth,0.9f,selected ? Cream : Muted);
            auto next=split+(name[split]==' ' ? 1 : 0);
            fittedText(out,name.substr(next),nameX,top+(compact ? 40 : 30),nameWidth,0.9f,selected ? Cream : Muted);
        } else fittedText(out,name,nameX,top+(compact ? 34 : 23),nameWidth,0.9f,selected ? Cream : Muted);
        hits.push_back({left,top,cellWidth,cellHeight,SectionUiPaletteItemBase+index});
    }
    if (!items.empty()) {
        const auto& item=items[selection];
        fittedText(out,item.name,x+23,y+341,320,1.4f,Gold);
        const auto hint=item.tool ? "CLICK TO ACTIVATE / G SIZE AND OPTIONS / X NORMAL PLACEMENT"
            : item.pickup ? "DROP ON GROUND / WALK OVER TO COLLECT"
            : item.asset && isSpawnerAsset(*item.asset) ? "PLACE / AIM AT SPAWNER AND PRESS E TO SET ENEMIES AND REWARDS"
            : item.asset && isChestAsset(*item.asset) ? "PLACE EMPTY / AIM AT CHEST AND PRESS E TO ADD ITEMS"
            : item.asset ? std::string(sectionItemGroup(item))+" / SOLID MODEL SURFACES" : item.block && *item.block==Block::Water ? "WATER / SWIM THROUGH"
            : item.block && isStair(*item.block) ? "HALF-HEIGHT STEPS / T ROTATE / WALK UP WITHOUT JUMPING"
            : item.marker==SectionMarker::Start || item.marker==SectionMarker::End ? "ONE PER SECTION - PLACING MOVES IT"
            : item.marker!=SectionMarker::None ? "PLACE ON A SAFE WALKABLE SURFACE"
            : item.block && isLeafBlock(*item.block) ? "LEAF CUTOUTS / SOLID FOR WALKING"
            : "BUILD WALKABLE SURFACES WITH BLOCKS";
        fittedText(out,hint,x+23,y+359,405,0.9f,Muted);
    }
    if (editor.paletteTab()==3) {
        sectionButton(out,hits,x+492,y+340,32,29,"-",SectionUiPickupMinus,false,editor.pickupQuantity()>1);
        out.frame(x+530,y+340,108,29,Edge,Canvas);
        out.centered("STACK "+std::to_string(editor.pickupQuantity()),x+584,y+350,1,Gold);
        sectionButton(out,hits,x+646,y+340,32,29,"+",SectionUiPickupPlus,false,editor.pickupQuantity()<ChestStackLimit);
        out.centered("SHIFT: +/- 10",x+584,y+374,.8f,Muted);
    } else {
        sectionButton(out,hits,x+w-208,y+340,43,29,"<",SectionUiPalettePrevious,false,page>0);
        out.centered(std::to_string(page+1)+" / "+std::to_string(std::max(1,(static_cast<int>(items.size())+23)/24)),x+w-118,y+350,1,Muted);
        sectionButton(out,hits,x+w-65,y+340,43,29,">",SectionUiPaletteNext,false,(page+1)*24<static_cast<int>(items.size()));
    }
    sectionHotbar(out,hits,editor,width*0.5f,y+384,true);
    out.centered(editor.paletteTab()==4 ? "ROUND CARVE: TUNNELS / BOX CARVE: ROOMS / LEFT CLICK DIGS"
        : "TAB - CATEGORY / ARROWS - ITEM / ENTER - EQUIP / 1-9 - SLOT",width*0.5f,y+444,0.9f,Muted);
    sectionFeedback(out,editor,x+23,y+461,w-46);
}

std::string sectionDimensions(glm::ivec3 size)
{
    return std::to_string(size.x)+" X "+std::to_string(size.y)+" X "+std::to_string(size.z);
}

std::string sectionCell(glm::ivec3 point)
{
    return std::to_string(point.x)+", "+std::to_string(point.y)+", "+std::to_string(point.z);
}

void sectionToolsMenu(Drawing& out,std::vector<SectionHit>& hits,const SectionEditor& editor,float width,float height)
{
    constexpr float w=700,h=478;
    const float x=(width-w)*0.5f,y=(height-h)*0.5f;
    sectionPanel(out,width,height,x,y,w,h,"BUILDING TOOLS","BUILD TERRAIN IN VOLUMES / COPY YOUR FAVORITE PIECES");
    const auto button=[&](float left,float top,float width,float height,std::string_view label,int id,
                          bool selected=false,bool enabled=true) {
        sectionButton(out,hits,left,top,width,height,label,id,selected,enabled,1.25f);
    };
    const std::array<std::string_view,3> modes{"SINGLE BLOCK","SELECT AREA","CUBE BRUSH"};
    for (int mode=0;mode<3;++mode)
        button(x+22+mode*222,y+64,212,29,modes[mode],SectionUiModeBase+mode,
                      static_cast<int>(editor.buildMode())==mode);
    out.text("BRUSH WIDTH",x+24,y+112,1.2f,Muted);
    constexpr std::array sizes{1,3,5,9};
    for (int index=0;index<4;++index)
        button(x+145+index*65,y+103,56,26,std::to_string(sizes[index]),
                      SectionUiBrushSizeBase+index,editor.brushSize()==sizes[index]);
    fittedText(out,"BLOCKS / ONE UNDO PER STAMP",x+422,y+112,252,1.15f,Muted);
    fittedText(out,"SELECT AREA: LEFT CLICK = CORNER A / RIGHT CLICK = CORNER B",x+24,y+143,w-48,1.15f,Cream);
    fittedText(out,"IN EMPTY AIR: J = CORNER A / K = CORNER B AT YOUR FLYING POSITION",x+24,y+161,w-48,1.15f,Muted);
    out.frame(x+22,y+183,w-44,72,Edge);
    const auto selection=editor.buildSelection();
    if (selection) {
        const auto size=selection->max-selection->min+1;
        fittedText(out,"SELECTION / "+sectionDimensions(size)+" / "+std::to_string(size.x*size.y*size.z)+" CELLS",x+34,y+195,430,1.2f,Leaf);
        fittedText(out,"FROM "+sectionCell(selection->min)+" TO "+sectionCell(selection->max),x+34,y+215,425,1.15f,Muted);
    } else {
        fittedText(out,editor.firstCorner() ? "CORNER A SET / CHOOSE CORNER B" : "PICK TWO CORNERS TO SELECT AN AREA",x+34,y+195,430,1.15f,Leaf);
        fittedText(out,"THE OUTLINE SHOWS EXACTLY WHICH BLOCKS ARE INCLUDED",x+34,y+215,430,1.15f,Muted);
    }
    fittedText(out,"HEIGHT",x+w-189,y+194,155,1.15f,Muted);
    button(x+w-189,y+213,70,27,"- 1",SectionUiHeightDown,false,selection.has_value());
    button(x+w-111,y+213,70,27,"+ 1",SectionUiHeightUp,false,selection.has_value());
    const auto& item=editor.hotbar()[editor.selectedSlot()];
    fittedText(out,item.block ? "FILL / "+item.name+" / E ITEMS" : "E: CHOOSE A BLOCK / 1-9: HOTBAR SLOT",x+34,y+237,430,1.15f,item.block ? Gold : Muted);
    button(x+22,y+267,212,32,"FILL SOLID",SectionUiFill,false,selection && item.block.has_value());
    button(x+244,y+267,212,32,"BUILD SHELL",SectionUiShell,false,selection && item.block.has_value());
    button(x+466,y+267,212,32,"CLEAR AREA",SectionUiErase,false,selection.has_value());
    fittedText(out,"SHELL BUILDS BOUNDARY / CLEAR REMOVES BLOCKS AND ASSETS / MARKERS STAY",x+24,y+307,w-48,1.15f,Muted);
    button(x+22,y+329,212,32,"COPY AREA",SectionUiCopy,false,selection.has_value());
    button(x+244,y+329,212,32,"PASTE PREVIEW",SectionUiPaste,editor.buildMode()==SectionBuildMode::Paste,!editor.clipboard().empty());
    button(x+466,y+329,212,32,"PASTE SETTINGS",SectionUiPasteSettings);
    const auto& clipboard=editor.clipboard();
    fittedText(out,clipboard.empty() ? "CLIPBOARD EMPTY / COPY BLOCKS AND ASSETS TOGETHER"
                : "COPIED / "+sectionDimensions(clipboard.size)+" / "+std::to_string(clipboard.assets.size())+" ASSETS / START AND END STAY",
                x+24,y+371,w-48,1.15f,clipboard.empty() ? Muted : Leaf);
    fittedText(out,"PASTE SETTINGS: FILL GAPS / LOCK HEIGHT / MOVE PREVIEW ONE BLOCK",x+24,y+390,w-48,1.15f,Muted);
    sectionFeedback(out,editor,x+24,y+413,w-48);
    button(x+22,y+h-42,212,28,"CLEAR SELECTION",SectionUiClearSelection,false,editor.firstCorner().has_value() || editor.secondCorner().has_value());
    button(x+w-234,y+h-42,212,28,"G / ESC - BUILD",SectionUiBack,true);
}

void sectionPasteMenu(Drawing& out,std::vector<SectionHit>& hits,const SectionEditor& editor,float width,float height)
{
    constexpr float w=700,h=478;
    const float x=(width-w)*0.5f,y=(height-h)*0.5f;
    sectionPanel(out,width,height,x,y,w,h,"PASTE SETTINGS","CHOOSE HOW YOUR COPIED PIECE FITS INTO THE WORLD");
    const bool copied=!editor.clipboard().empty();
    sectionButton(out,hits,x+22,y+66,322,34,"STANDARD PASTE",SectionUiPasteModeBase,!editor.pasteFillGaps(),copied,1.3f);
    sectionButton(out,hits,x+356,y+66,322,34,"FILL GAPS",SectionUiPasteModeBase+1,editor.pasteFillGaps(),copied,1.3f);
    fittedText(out,"COPY BLOCKS AND DECORATIONS",x+30,y+116,306,1.15f,Cream);
    fittedText(out,"SNAP TO THE AIMED SURFACE",x+30,y+138,306,1.0f,Muted);
    fittedText(out,"REPLACE EXISTING BLOCKS",x+30,y+158,306,1.0f,Muted);
    fittedText(out,"ADD BLOCKS ONLY IN EMPTY SPACES",x+364,y+116,306,1.15f,Leaf);
    fittedText(out,"KEEP BLOCKS AND DECORATIONS",x+364,y+138,306,1.0f,Muted);
    fittedText(out,"SKIP COPIED MODELS",x+364,y+158,306,1.0f,Muted);
    out.frame(x+22,y+190,w-44,60,Edge);
    sectionButton(out,hits,x+34,y+204,194,30,editor.pasteIncludesAir() ? "WITH AIR: ON" : "WITH AIR: OFF",
                  SectionUiPasteAir,editor.pasteIncludesAir() && !editor.pasteFillGaps(),!editor.pasteFillGaps(),1.15f);
    fittedText(out,"STANDARD ONLY: COPY EMPTY SPACE TOO",x+246,y+204,416,1.1f,Cream);
    fittedText(out,"WITH AIR ALSO CLEARS DECORATIONS IN THE STAMP",x+246,y+225,416,1.0f,Muted);
    fittedText(out,"FILL GAPS: AIM AT A FLOOR TOP / LEFT CLICK OR L TO LOCK ITS HEIGHT",x+24,y+271,w-48,1.1f,Leaf);
    fittedText(out,"MOVE THE PREVIEW ACROSS THE FLOOR / ONLY NEW BLOCKS GLOW GREEN",x+24,y+293,w-48,1.1f,Muted);
    fittedText(out,"ARROWS MOVE ONE BLOCK / PAGE UP-DOWN MOVE VERTICALLY",x+24,y+322,w-48,1.15f,Cream);
    fittedText(out,"FIRST NUDGE PINS THE PREVIEW / P TOGGLE PIN OR FOLLOW AIM",x+24,y+344,w-48,1.1f,Muted);
    fittedText(out,"RIGHT CLICK STAMP / T ROTATE / X CANCEL",x+24,y+366,w-48,1.15f,Muted);
    fittedText(out,copied ? "COPIED / "+sectionDimensions(editor.clipboard().size)
                         : "CLIPBOARD EMPTY / SELECT AND COPY AN AREA FIRST",x+24,y+394,w-48,1.1f,copied ? Leaf : Gold);
    sectionFeedback(out,editor,x+24,y+414,w-48);
    sectionButton(out,hits,x+22,y+h-42,212,28,"ESC - BACK TO TOOLS",SectionUiBack);
    sectionButton(out,hits,x+w-260,y+h-42,238,28,"RESUME PREVIEW",SectionUiPasteResume,true,copied);
}

void sectionAcceptMenu(Drawing& out,std::vector<SectionHit>& hits,const SectionEditor& editor,float width,float height)
{
    constexpr float w=620,h=454;
    const float x=(width-w)*0.5f,y=(height-h)*0.5f;
    sectionPanel(out,width,height,x,y,w,h,"ADD TO WORLD",sectionWorldName(editor.selectedWorld())+" / NAME AND CLASSIFY THIS SECTION");
    out.text(editor.nameSelected() ? "SECTION NAME / TYPE TO REPLACE SELECTED NAME" : "SECTION NAME / CTRL+A SELECTS ALL",x+23,y+72,1,Muted);
    out.frame(x+22,y+88,w-44,36,Gold,Canvas);
    if (editor.nameSelected()) out.quad(x+31,y+95,std::min(w-62,editor.meta().name.size()*9.0f+10),22,{.34f,.27f,.17f,1});
    fittedText(out,editor.meta().name+(editor.nameSelected() ? "" : "_"),x+35,y+100,w-70,1.5f,Cream);
    out.text("CATEGORY",x+23,y+143,1,Muted);
    const auto categories=sectionCategoryCatalog();
    for (int index=0;index<static_cast<int>(categories.size());++index) {
        const auto& category=categories[index];
        const float top=y+163+index*37;
        const bool selected=editor.meta().category==category.id;
        out.frame(x+22,top,w-44,31,selected ? Gold : Edge,selected ? Selected : Panel);
        out.text(selected ? ">" : "-",x+33,top+10,1.2f,selected ? Gold : Muted);
        fittedText(out,category.name,x+51,top+10,142,1.1f,selected ? Gold : Cream);
        fittedText(out,category.description,x+210,top+11,w-246,0.9f,Muted);
        hits.push_back({x+22,top,w-44,31,SectionUiCategoryBase+index});
    }
    out.text(editor.meta().tested ? "PLAYTEST PASSED / READY TO ADD" : "COMPLETE A PLAYTEST BEFORE ACCEPTING",x+24,y+357,1,editor.meta().tested ? Leaf : Gold);
    sectionFeedback(out,editor,x+24,y+377,w-48);
    sectionButton(out,hits,x+22,y+h-51,175,30,"ESC - KEEP EDITING",SectionUiBack);
    sectionButton(out,hits,x+w-275,y+h-51,253,30,"ENTER - ACCEPT SECTION",SectionUiConfirm,true,editor.meta().tested);
    out.centered("TYPE NAME / CTRL+A SELECT ALL / UP-DOWN CATEGORY",width*0.5f,y+h-13,0.8f,Muted);
}

void sectionChestMenu(Drawing& out,std::vector<SectionHit>& hits,const SectionEditor& editor,float width,float height)
{
    constexpr float w=700,h=478;
    const float x=(width-w)*.5f,y=(height-h)*.5f;
    const bool locked=editor.editingChestKind()==HubAssetKind::IronboundChest;
    const bool rewards=isSpawnerAsset(editor.editingChestKind());
    sectionPanel(out,width,height,x,y,w,h,rewards ? "TRIAL REWARDS" : "CHEST CONTENTS",
        rewards ? "ITEMS POP OUT OF THE SPAWNER WHEN EVERY ENEMY IS DEFEATED"
            : std::string(chestAssetName(editor.editingChestKind()))
                +(locked ? " / REQUIRES A CAVE KEY" : " / EXACT ITEMS SAVED WITH THIS CHEST"));
    const auto& slots=editor.chestSlots();
    const int selection=std::clamp(editor.chestSelection(),0,ChestSlotCount-1);
    const auto& selected=slots[selection];
    const bool occupied=selected.count>0 && validItem(selected.item);
    const auto used=std::count_if(slots.begin(),slots.end(),[](const ItemStack& stack){return stack.count>0;});
    out.text("ITEM CATALOG",x+22,y+73,1,Muted);
    out.text(std::string(rewards ? "REWARDS / " : "CHEST / ")+std::to_string(used)+" OF 20 SLOTS",x+324,y+73,1,Gold);
    // Two columns leave enough width for readable names even on small displays.
    const auto catalog=itemCatalog();
    for (std::size_t index=0;index<catalog.size();++index) {
        const auto& item=catalog[index];
        const float left=x+22+(index%2)*146,top=y+95+(index/2)*30;
        const bool active=occupied && selected.item==item.id;
        out.frame(left,top,140,28,active ? Gold : Edge,active ? Selected : Panel,4);
        oreIcon(out,item.id,{left+15,top+14},.52f);
        const auto split=item.name.find(' ');
        if (split==std::string_view::npos)
            fittedText(out,item.name,left+30,top+10,104,.9f,active ? Cream : Muted);
        else {
            fittedText(out,item.name.substr(0,split),left+30,top+4,104,.9f,active ? Cream : Muted);
            fittedText(out,item.name.substr(split+1),left+30,top+16,104,.8f,Muted);
        }
        hits.push_back({left,top,140,28,SectionUiChestItemBase+static_cast<int>(item.id)});
    }
    for (int slot=0;slot<ChestSlotCount;++slot) {
        const float left=x+324+(slot%5)*66,top=y+95+(slot/5)*66;
        const auto& stack=slots[slot];
        out.frame(left,top,60,60,slot==selection ? Gold : Edge,slot==selection ? Selected : Panel,4);
        out.text(std::to_string(slot+1),left+5,top+5,1,Muted);
        if (stack.count>0 && validItem(stack.item)) {
            oreIcon(out,stack.item,{left+30,top+29},.82f);
            const auto count=std::to_string(stack.count);
            const float countWidth=(count.size()*6-1)*out.fontPixel(1);
            out.quad(left+54-countWidth,top+45,countWidth+3,12,Panel);
            out.text(count,left+54-countWidth,top+48,1,Cream);
        } else {
            out.text("+",left+27,top+27,1,Muted);
        }
        hits.push_back({left,top,60,60,SectionUiChestSlotBase+slot});
    }
    fittedText(out,occupied ? std::string(itemInfo(selected.item).name)+" / QUANTITY 1-99"
                           : "EMPTY SLOT / CHOOSE AN ITEM",
               x+324,y+363,324,1,occupied ? Gold : Muted);
    sectionButton(out,hits,x+324,y+383,35,28,"-",SectionUiChestMinus,false,occupied);
    sectionButton(out,hits,x+364,y+383,72,28,editor.chestQuantityText(),
                  SectionUiChestQuantity,editor.chestQuantitySelected(),occupied);
    sectionButton(out,hits,x+441,y+383,35,28,"+",SectionUiChestPlus,false,occupied);
    sectionButton(out,hits,x+484,y+383,164,28,"REMOVE ITEM",SectionUiChestRemove,false,occupied);
    fittedText(out,"SLOT / ITEM / TYPE AMOUNT",x+22,y+400,286,1,Muted);
    sectionFeedback(out,editor,x+22,y+422,w-44);
    sectionButton(out,hits,x+22,y+441,140,27,"CLEAR ALL",SectionUiChestClear,false,used>0);
    sectionButton(out,hits,x+324,y+441,132,27,"ESC - CANCEL",SectionUiChestCancel);
    sectionButton(out,hits,x+466,y+441,212,27,rewards ? "ENTER - DONE" : "ENTER - SAVE CONTENTS",SectionUiChestApply,true);
}

void sectionSpawnerMenu(Drawing& out,std::vector<SectionHit>& hits,const SectionEditor& editor,float width,float height)
{
    constexpr float w=610,h=440;
    const float x=(width-w)*.5f,y=(height-h)*.5f;
    sectionPanel(out,width,height,x,y,w,h,"TRIAL SPAWNER","ACTIVATES AUTOMATICALLY WHEN A PLAYER COMES WITHIN 8 BLOCKS");
    const auto& counts=editor.spawnerCounts();
    constexpr std::array<std::string_view,3> labels{"SKELETON","WIZARD","SLIME"};
    constexpr std::array<std::string_view,3> details{"CHASES AND SLASHES","CASTS DODGEABLE BOLTS","HOPS TOWARD THE PLAYER"};
    constexpr std::array<glm::vec4,3> tints{Cream,glm::vec4(.74f,.52f,.94f,1),Leaf};
    int total=0;
    for(int kind=0;kind<3;++kind) {
        total+=counts[kind];
        const float top=y+81+kind*66;
        out.frame(x+22,top,w-44,57,Edge,Canvas,5);
        out.quad(x+24,top+9,3,39,tints[kind]);
        out.text(labels[kind],x+40,top+11,1.5f,tints[kind]);
        out.text(details[kind],x+40,top+34,1,Muted);
        sectionButton(out,hits,x+w-180,top+13,33,31,"-",SectionUiSpawnerMinusBase+kind,false,counts[kind]>0);
        out.centered(std::to_string(counts[kind]),x+w-112,top+23,1.5f,Cream);
        sectionButton(out,hits,x+w-75,top+13,33,31,"+",SectionUiSpawnerPlusBase+kind,false,counts[kind]<SpawnerMobLimit);
    }
    out.text(std::to_string(total)+(total==1 ? " ENEMY TOTAL" : " ENEMIES TOTAL"),x+24,y+290,1.25f,Gold);
    out.text("UP TO 8 ACTIVE AT ONCE",x+w-230,y+293,1,Muted);
    sectionButton(out,hits,x+22,y+318,w-44,35,
        "EDIT REWARDS / "+std::to_string(editor.spawnerRewards().size())
            +(editor.spawnerRewards().size()==1 ? " ITEM STACK" : " ITEM STACKS"),SectionUiSpawnerRewards);
    fittedText(out,"CLEARS PERMANENTLY / UNFINISHED TRIALS RESET ON DEATH",x+24,y+365,w-48,1,Muted);
    sectionFeedback(out,editor,x+24,y+384,w-48);
    sectionButton(out,hits,x+22,y+405,172,27,"ESC - CANCEL",SectionUiSpawnerCancel);
    sectionButton(out,hits,x+w-273,y+405,251,27,"ENTER - SAVE SPAWNER",SectionUiSpawnerApply,true,total>0);
}

void sectionBuildHud(Drawing& out,std::vector<SectionHit>& hits,const Game& game,float width,float height)
{
    const auto& editor=game.sectionEditor();
    const bool testing=editor.screen()==SectionEditorScreen::Test;
    const float center=width*0.5f;
    constexpr float w=620;
    const float x=center-w*0.5f;
    if (testing) {
        quietText(out,"PLAYTEST / "+editor.meta().name,18,20,width-300);
        quietText(out,editor.testComplete() ? "COMPLETE / F10 ACCEPT / F8 BUILD"
            : editor.checkpointReached() ? "CHECKPOINT SAVED / F8 BUILD" : "F8 BACK TO BUILD",18,36,width-36,
            editor.testComplete() || editor.checkpointReached() ? Leaf : Muted);
        toolBelt(out,game.equipment,center,height-64);
        quietText(out,"H HELP",width-82,height-27,64,Muted);
    } else {
        out.frame(x,14,w,66,Edge);
        out.quad(x+2,16,3,62,Leaf);
        out.text("SECTION WORKSHOP / CREATIVE",x+16,25,1,Leaf);
        fittedText(out,editor.meta().name,x+16,43,380,1.5f,Cream);
        fittedText(out,sectionWorldName(editor.selectedWorld()),x+w-205,25,188,1,Muted);
        out.text(editor.dirty() ? "DRAFT / UNSAVED CHANGES" : "DRAFT SAVED",x+w-205,45,1,Muted);
        fittedText(out,editor.message(),x+16,67,w-32,0.85f,Leaf);
        const auto& item=editor.hotbar()[editor.selectedSlot()];
        const auto mode=editor.buildMode();
        const bool pasting=mode==SectionBuildMode::Paste;
        const float labelY=height-(pasting ? 164 : 129);
        out.frame(center-280,labelY-6,560,pasting ? 74 : 39,Edge,Panel);
        const auto title=mode==SectionBuildMode::Select ? "SELECT AREA / PICK TWO CORNERS"
            : pasting ? std::string(editor.pasteFillGaps() ? "FILL GAPS / " : "PASTE / ")+sectionDimensions(rotatedSectionSize(editor.clipboard(),editor.pasteRotation()))
                +" / "+std::to_string(editor.pasteRotation()*90)+" DEG"
            : mode==SectionBuildMode::Brush ? std::string(editor.brushErasing() ? "CARVE " : "ADD ")+std::to_string(editor.brushSize())+" / "+item.name
            : item.pickup ? item.name+" / STACK "+std::to_string(editor.pickupQuantity()) : item.name;
        fittedText(out,title,center-264,labelY,528,1.4f,Gold);
        const auto help=mode==SectionBuildMode::Select ? "LEFT CORNER A / RIGHT CORNER B / J-K AIR / G APPLY"
            : pasting ? "RIGHT STAMP / T ROTATE / X CANCEL / G SETTINGS"
            : mode==SectionBuildMode::Brush ? "LEFT ERASE CUBE / RIGHT PLACE CUBE / G SIZE"
            : item.pickup ? "RIGHT DROP ITEM / WALK OVER TO COLLECT IN PLAY"
            : item.asset && isSpawnerAsset(*item.asset) ? "RIGHT PLACE / E SET ENEMIES AND REWARDS"
            : item.asset && isChestAsset(*item.asset) ? "T ROTATE / PLACE EMPTY / E EDIT PLACED CHEST"
            : item.asset == HubAssetKind::GardenGate ? std::string("T ROTATE / Y OPEN-CLOSED / ")+(editor.variation()%2 ? "OPEN" : "CLOSED")
            : item.block && isStair(*item.block) ? "T ROTATE "+std::to_string(stairFacing(*item.block)*90)+" DEG / RIGHT PLACE STAIR"
            : item.asset && buildingGridSize(*item.asset).x ? "T ROTATE / Y FINISH / "
                +sectionDimensions(buildingGridSize(*item.asset,editor.facing()))+" BLOCK GRID"
            : item.asset ? "T ROTATE "+std::to_string(editor.facing()*90)+" DEG / Y VARIANT "+std::to_string(editor.variation())
            : item.marker!=SectionMarker::None ? "RIGHT CLICK A SAFE FLOOR TO PLACE MARKER" : "LEFT REMOVE / RIGHT PLACE / MIDDLE PICK";
        if (pasting) {
            std::string status;
            if (editor.pasteFillGaps() && !editor.pasteFloorHeight()) status="AIM AT A FLOOR TOP / LEFT CLICK OR L TO LOCK HEIGHT";
            else {
                if (editor.pasteFillGaps()) status="LEVEL Y "+std::to_string(*editor.pasteFloorHeight())+" / ";
                status+=editor.pastePinned() ? "PINNED" : "FOLLOW AIM";
                status+=editor.toolPreview() ? " / "+std::to_string(editor.previewBlockChanges().size())+(editor.pasteFillGaps() ? " BLOCKS TO ADD" : " BLOCK CHANGES")
                    : editor.pasteFillGaps() ? " / AIM TOWARD THE LOCKED FLOOR" : " / AIM AT A SURFACE";
            }
            fittedText(out,status,center-264,labelY+20,528,1.0f,Leaf);
            fittedText(out,"ARROWS NUDGE / PG UP-DOWN HEIGHT / P PIN-FOLLOW",center-264,labelY+38,528,.95f,Cream);
            fittedText(out,help,center-264,labelY+55,528,.9f,Muted);
        } else fittedText(out,help,center-264,labelY+20,528,.9f,Muted);
        sectionHotbar(out,hits,editor,center,height-86,false);
        out.centered("G TOOLS / E ITEMS / F8 TEST / F10 ACCEPT / F5 SAVE / F9 LIBRARY",center,height-20,.9f,Cream);
        if (const auto target=editor.targetedAsset(); target && (isChestAsset(target->kind) || isSpawnerAsset(target->kind))) {
            out.frame(center-180,height*.5f+31,360,30,Gold);
            fittedText(out,"E EDIT "+std::string(isSpawnerAsset(target->kind) ? "TRIAL SPAWNER" : chestAssetName(target->kind)),center-166,height*.5f+41,332,1,Gold);
        }
        out.frame(15,97,264,119,Edge);
        fittedText(out,"WASD FLY / SPACE UP / CTRL DOWN",27,109,240,.95f,Cream);
        fittedText(out,"SHIFT FAST / ALT SLOW / 1-9 SLOTS",27,127,240,.9f,Muted);
        fittedText(out,"CMD/CTRL+Z UNDO / +Y REDO",27,145,240,.95f,Muted);
        fittedText(out,"CMD/CTRL+C COPY / +V PASTE",27,163,240,.95f,Muted);
        if (const auto selection=editor.buildSelection())
            fittedText(out,"AREA "+sectionDimensions(selection->max-selection->min+1)+" / G EDIT",27,181,240,.95f,Leaf);
        else fittedText(out,editor.firstCorner() ? "CORNER A SET / PICK CORNER B" : "G TOOLS / SELECT, FILL, COPY",27,181,240,.95f,Leaf);
        fittedText(out,"64 X 48 X 64 / PROTECTED SHELL",27,199,240,.85f,Muted);
    }
}

void sectionUi(Drawing& out,std::vector<SectionHit>& hits,const Game& game,float width,float height)
{
    const auto& editor=game.sectionEditor();
    switch (editor.screen()) {
    case SectionEditorScreen::Worlds: sectionWorldMenu(out,hits,editor,width,height); break;
    case SectionEditorScreen::Library: sectionLibraryMenu(out,hits,editor,width,height); break;
    case SectionEditorScreen::Palette: sectionPaletteMenu(out,hits,editor,width,height); break;
    case SectionEditorScreen::Accept: sectionAcceptMenu(out,hits,editor,width,height); break;
    case SectionEditorScreen::Tools: sectionToolsMenu(out,hits,editor,width,height); break;
    case SectionEditorScreen::PasteSettings: sectionPasteMenu(out,hits,editor,width,height); break;
    case SectionEditorScreen::ChestContents: sectionChestMenu(out,hits,editor,width,height); break;
    case SectionEditorScreen::SpawnerSettings: sectionSpawnerMenu(out,hits,editor,width,height); break;
    case SectionEditorScreen::Build:
    case SectionEditorScreen::Test: sectionBuildHud(out,hits,game,width,height); break;
    }
}


// All level menus and their pointer targets use this one logical canvas.
SectionCanvas levelCanvas(int width,int height,float contentScale)
{
    if (!std::isfinite(contentScale) || contentScale<=0) contentScale=1;
    const float scale=std::min({std::clamp(contentScale,.5f,4.0f),width/776.0f,height/550.0f});
    return {scale,width/scale,height/scale};
}

void levelFeedback(Drawing& out,const LevelController& level,float x,float y,float width)
{
    std::string text=level.message();
    std::replace(text.begin(),text.end(),'\n',' ');
    fittedText(out,text,x,y,width,1,Leaf);
}

void levelWorldMenu(Drawing& out,std::vector<SectionHit>& hits,const LevelController& level,float width,float height)
{
    constexpr float w=620,h=410;
    const float x=(width-w)*.5f,y=(height-h)*.5f;
    sectionPanel(out,width,height,x,y,w,h,level.creator() ? "LEVEL CREATOR" : "EXPEDITIONS",
                 level.creator() ? "CHOOSE THE WORLD FOR YOUR LEVEL" : "CHOOSE A WORLD TO EXPLORE");
    const auto worlds=sectionWorldCatalog();
    for (int index=0;index<static_cast<int>(worlds.size());++index) {
        const float top=y+72+index*78;
        const bool selected=index==level.selection();
        const auto color=index==0 ? Leaf : index==1 ? glm::vec4(.36f,.79f,.94f,1) : glm::vec4(.98f,.53f,.25f,1);
        out.frame(x+22,top,w-44,68,selected ? Gold : Edge,selected ? Selected : Panel);
        out.quad(x+30,top+10,4,48,color);
        out.text("0"+std::to_string(index+1),x+48,top+24,2,color);
        fittedText(out,worlds[index].name,x+94,top+13,w-130,1.8f,Cream);
        fittedText(out,worlds[index].description,x+95,top+42,w-133,1,Muted);
        hits.push_back({x+22,top,w-44,68,LevelUiWorldBase+index});
    }
    fittedText(out,level.creator() ? "ACCEPTED SECTIONS BECOME AN ORDERED ADVENTURE" : "YOUR PUBLISHED LEVELS ARE READY TO PLAY HERE",x+24,y+319,w-48,1,Leaf);
    levelFeedback(out,level,x+24,y+339,w-48);
    sectionButton(out,hits,x+22,y+365,158,28,"ESC - BACK",LevelUiBack);
    sectionButton(out,hits,x+w-242,y+365,220,28,"ENTER - CHOOSE WORLD",LevelUiPlay,true);
}

void levelLibraryMenu(Drawing& out,std::vector<SectionHit>& hits,const LevelController& level,float width,float height)
{
    constexpr float w=700,h=478;
    const float x=(width-w)*.5f,y=(height-h)*.5f;
    sectionPanel(out,width,height,x,y,w,h,level.creator() ? "LEVEL CREATOR / LIBRARY" : "CHOOSE YOUR LEVEL",
                 sectionWorldName(level.selectedWorld())+" / "+(level.creator() ? "COMPOSE AND PUBLISH" : "PUBLISHED EXPEDITIONS"));
    if (level.creator()) {
        sectionButton(out,hits,x+22,y+65,318,32,"+ NEW LEVEL",LevelUiNew,level.selection()<0);
        sectionButton(out,hits,x+352,y+65,326,32,"RESUME SAVED COMPOSITION",LevelUiResume,false,level.draft().has_value());
    } else {
        fittedText(out,"REACH EACH END MARKER TO CONTINUE TO THE NEXT SECTION",x+24,y+75,w-48,1,Leaf);
    }
    const auto& entries=level.entries();
    for (int row=0;row<LevelLibraryPageSize;++row) {
        const int index=level.libraryPage()*LevelLibraryPageSize+row;
        if (index>=static_cast<int>(entries.size())) break;
        const auto& entry=entries[index].definition;
        const float top=y+108+row*39;
        const bool selected=index==level.selection();
        out.frame(x+22,top,w-44,34,selected ? Gold : Edge,selected ? Selected : Panel);
        fittedText(out,"LEVEL "+std::to_string(entry.number),x+35,top+10,98,1.1f,selected ? Gold : Leaf);
        fittedText(out,entry.name,x+145,top+7,388,1.25f,Cream);
        fittedText(out,std::to_string(entry.sections.size())+" SECTIONS",x+w-134,top+11,100,1,Muted);
        hits.push_back({x+22,top,w-44,34,LevelUiEntryBase+index});
    }
    if (entries.empty()) {
        out.centered("NO LEVELS HERE YET",width*.5f,y+211,1.6f,Cream);
        fittedText(out,level.creator() ? "CREATE A LEVEL FROM YOUR ACCEPTED SECTIONS." : "USE F11 TO CREATE AND PUBLISH THIS WORLD'S FIRST LEVEL.",x+70,y+241,w-140,1,Muted);
    }
    sectionButton(out,hits,x+22,y+388,100,26,"< PREVIOUS",LevelUiLibraryPrevious,false,level.libraryPage()>0);
    out.centered("PAGE "+std::to_string(level.libraryPage()+1)+" / "+std::to_string(std::max(1,(static_cast<int>(entries.size())+LevelLibraryPageSize-1)/LevelLibraryPageSize)),width*.5f,y+397,1,Muted);
    sectionButton(out,hits,x+w-122,y+388,100,26,"NEXT >",LevelUiLibraryNext,false,(level.libraryPage()+1)*LevelLibraryPageSize<static_cast<int>(entries.size()));
    levelFeedback(out,level,x+24,y+426,w-48);
    sectionButton(out,hits,x+22,y+h-38,158,27,"ESC - WORLDS",LevelUiBack);
    sectionButton(out,hits,x+w-262,y+h-38,240,27,level.creator() ? "ENTER - EDIT LEVEL" : "ENTER - START LEVEL",LevelUiPlay,true,!entries.empty());
}

void levelField(Drawing& out,std::vector<SectionHit>& hits,const LevelController& level,
                float x,float y,float width,std::string_view value,LevelTextField field,int action)
{
    const bool active=level.textField()==field;
    out.frame(x,y,width,31,active ? Gold : Edge,Canvas);
    if (active && level.textSelected()) out.quad(x+7,y+6,width-14,19,{.34f,.27f,.17f,1});
    fittedText(out,std::string(value)+(active && !level.textSelected() ? "_" : ""),x+10,y+10,width-20,1.35f,Cream);
    hits.push_back({x,y,width,31,action});
}

void levelComposeMenu(Drawing& out,std::vector<SectionHit>& hits,const LevelController& level,float width,float height)
{
    constexpr float w=740,h=510,column=334;
    const float x=(width-w)*.5f,y=(height-h)*.5f,left=x+22,right=x+384;
    const auto& definition=level.definition();
    sectionPanel(out,width,height,x,y,w,h,"COMPOSE A LEVEL",sectionWorldName(definition.world)+" / CONNECT EACH END TO THE NEXT START");
    out.text("LEVEL NAME",left,y+66,1,Muted);
    out.text("WORLD POSITION / 1-999",x+486,y+66,1,Muted);
    levelField(out,hits,level,left,y+80,450,definition.name,LevelTextField::Name,LevelUiName);
    levelField(out,hits,level,x+486,y+80,232,level.numberText(),LevelTextField::Number,LevelUiNumber);
    out.text("ACCEPTED SECTIONS / CLICK TO ADD",left,y+126,1,Leaf);
    fittedText(out,"PLAY ORDER / "+std::to_string(definition.sections.size())+" OF "+std::to_string(LevelMaxSections),right,y+126,column,1,Gold);
    for (int row=0;row<LevelSectionPageSize;++row) {
        const int index=level.sectionPage()*LevelSectionPageSize+row;
        if (index>=static_cast<int>(level.sections().size())) break;
        const auto& section=level.sections()[index];
        const float top=y+146+row*34;
        const bool selected=index==level.selection();
        out.frame(left,top,column,30,selected ? Gold : Edge,selected ? Selected : Panel);
        out.text("+",left+10,top+9,1.3f,Leaf);
        fittedText(out,section.meta.name,left+29,top+5,column-42,1,Cream);
        fittedText(out,sectionCategoryName(section.meta.category),left+29,top+18,column-42,.8f,Muted);
        if (definition.sections.size()<LevelMaxSections)
            hits.push_back({left,top,column,30,LevelUiSectionBase+index});
    }
    for (int row=0;row<LevelQueuePageSize;++row) {
        const int index=level.queuePage()*LevelQueuePageSize+row;
        if (index>=static_cast<int>(definition.sections.size())) break;
        const float top=y+146+row*34;
        const bool selected=index==level.selectedQueue();
        out.frame(right,top,column,30,selected ? Gold : Edge,selected ? Selected : Panel);
        out.quad(right+5,top+4,29,22,selected ? Gold : Canvas);
        out.centered(std::to_string(index+1),right+19.5f,top+11,1,selected ? Panel : Cream);
        fittedText(out,level.sectionName(definition.sections[index]),right+43,top+10,column-57,1.15f,Cream);
        hits.push_back({right,top,column,30,LevelUiQueueBase+index});
    }
    if (level.sections().empty()) {
        fittedText(out,"NO ACCEPTED SECTIONS IN THIS WORLD",left+10,y+197,column-20,1,Cream);
        fittedText(out,"F7 OPENS THE SECTION WORKSHOP.",left+10,y+222,column-20,1,Muted);
        fittedText(out,"BUILD, TEST, AND ACCEPT A SECTION FIRST.",left+10,y+245,column-20,1,Muted);
    }
    if (definition.sections.empty()) {
        fittedText(out,"YOUR ROUTE STARTS HERE",right+16,y+197,column-32,1.5f,Cream);
        fittedText(out,"ADD A SECTION FROM THE LEFT.",right+16,y+227,column-32,1,Muted);
        fittedText(out,"YOU CAN USE A SECTION MORE THAN ONCE.",right+16,y+250,column-32,1,Muted);
    }
    const auto pages=[&](float columnX,int page,int count,int size,int previous,int next) {
        sectionButton(out,hits,columnX,y+388,64,25,"<",previous,false,page>0);
        out.centered("PAGE "+std::to_string(page+1)+" / "+std::to_string(std::max(1,(count+size-1)/size)),columnX+column*.5f,y+397,1,Muted);
        sectionButton(out,hits,columnX+column-64,y+388,64,25,">",next,false,(page+1)*size<count);
    };
    pages(left,level.sectionPage(),static_cast<int>(level.sections().size()),LevelSectionPageSize,LevelUiSectionPrevious,LevelUiSectionNext);
    pages(right,level.queuePage(),static_cast<int>(definition.sections.size()),LevelQueuePageSize,LevelUiQueuePrevious,LevelUiQueueNext);
    fittedText(out,"TAB FOCUS / ARROWS / ENTER ADD",left,y+425,column,.9f,Muted);
    const int selected=level.selectedQueue();
    sectionButton(out,hits,right,y+420,105,25,"MOVE UP",LevelUiMoveUp,false,selected>0);
    sectionButton(out,hits,right+113,y+420,105,25,"MOVE DOWN",LevelUiMoveDown,false,selected>=0 && selected+1<static_cast<int>(definition.sections.size()));
    sectionButton(out,hits,right+226,y+420,108,25,"REMOVE",LevelUiRemove,false,selected>=0);
    levelFeedback(out,level,left,y+453,w-44);
    sectionButton(out,hits,left,y+474,130,27,"ESC - LIBRARY",LevelUiBack);
    sectionButton(out,hits,x+164,y+474,242,27,"PLAY THIS COMPOSITION",LevelUiPlay,false,!definition.sections.empty());
    sectionButton(out,hits,x+418,y+474,300,27,"SAVE AND PUBLISH LEVEL",LevelUiPublish,true,!definition.sections.empty());
}

void levelReplaceMenu(Drawing& out,std::vector<SectionHit>& hits,const LevelController& level,float width,float height)
{
    constexpr float w=570,h=258;
    const float x=(width-w)*.5f,y=(height-h)*.5f;
    sectionPanel(out,width,height,x,y,w,h,"REPLACE THIS WORLD'S LEVEL?",sectionWorldName(level.definition().world)+" / LEVEL "+std::to_string(level.definition().number));
    fittedText(out,"THAT LEVEL NUMBER IS ALREADY PUBLISHED.",x+24,y+84,w-48,1.4f,Gold);
    fittedText(out,"REPLACING IT CHANGES THE ROUTE PLAYERS WILL FIND THERE.",x+24,y+116,w-48,1,Muted);
    fittedText(out,"THE ACCEPTED SECTIONS THEMSELVES ARE KEPT.",x+24,y+139,w-48,1,Muted);
    fittedText(out,level.definition().name,x+24,y+172,w-48,1.5f,Cream);
    sectionButton(out,hits,x+22,y+h-49,246,30,"ESC - KEEP EDITING",LevelUiCancel);
    sectionButton(out,hits,x+280,y+h-49,268,30,"REPLACE LEVEL",LevelUiReplace,true);
}

void levelRunHud(Drawing& out,std::vector<SectionHit>& hits,const LevelController& level,float width,float height)
{
    const float center=width*.5f;
    const auto& definition=level.definition();
    if (level.screen()==LevelScreen::Play) {
        quietText(out,std::to_string(level.sectionIndex()+1)+" / "+std::to_string(definition.sections.size())
            +"   "+level.meta().name,18,20,width-300);
        if (level.checkpointReached()) quietText(out,"CHECKPOINT SAVED",18,36,width-36,Leaf);
    } else if (level.screen()==LevelScreen::Transition) {
        out.quad(0,0,width,height,{.025f,.016f,.009f,.78f});
        out.frame(center-244,height*.5f-56,488,112,Leaf);
        out.centered("SECTION COMPLETE",center,height*.5f-35,1.8f,Leaf);
        out.centered("TRAVELING TO THE NEXT START",center,height*.5f-5,1.15f,Cream);
        out.centered("SECTION "+std::to_string(level.sectionIndex()+2)+" / "+std::to_string(definition.sections.size()),center,height*.5f+23,1,Muted);
    } else if (level.screen()==LevelScreen::Complete) {
        const float top=height*.5f-78;
        out.quad(0,0,width,height,{.025f,.016f,.009f,.6f});
        out.frame(center-280,top,560,167,Leaf);
        out.centered("LEVEL COMPLETE",center,top+22,2,Leaf);
        out.centered("ALL "+std::to_string(definition.sections.size())+" SECTIONS FINISHED",center,top+54,1.3f,Cream);
        out.centered("TIME "+crossingTime(level.elapsed())+" / FALLS "+std::to_string(level.falls()),center,top+79,1,Muted);
        sectionButton(out,hits,center-257,top+119,247,30,"REPLAY LEVEL",LevelUiReplay);
        sectionButton(out,hits,center+10,top+119,247,30,"RETURN TO VILLAGE",LevelUiHub,true);
    }
}

void levelErrorMenu(Drawing& out,std::vector<SectionHit>& hits,const LevelController& level,float width,float height)
{
    constexpr float w=620,h=250;
    const float x=(width-w)*.5f,y=(height-h)*.5f;
    sectionPanel(out,width,height,x,y,w,h,"THIS SECTION COULD NOT LOAD","YOUR LEVEL DEFINITION AND ACCEPTED SECTIONS HAVE BEEN KEPT");
    wrappedDescription(out,level.message(),x+24,y+89,static_cast<std::size_t>((w-48)/(6*out.fontPixel(1))));
    fittedText(out,"RESTORE THE MISSING SECTION AND RETRY, OR RETURN TO THE HUB.",x+24,y+147,w-48,1,Muted);
    sectionButton(out,hits,x+22,y+h-49,278,30,"RETURN TO VILLAGE",LevelUiHub);
    sectionButton(out,hits,x+312,y+h-49,286,30,"RETRY LOADING",LevelUiRetry,true);
}

void levelUi(Drawing& out,std::vector<SectionHit>& hits,const Game& game,float width,float height)
{
    const auto& level=game.levelController();
    switch (level.screen()) {
    case LevelScreen::Worlds: levelWorldMenu(out,hits,level,width,height); break;
    case LevelScreen::Library: levelLibraryMenu(out,hits,level,width,height); break;
    case LevelScreen::Compose: levelComposeMenu(out,hits,level,width,height); break;
    case LevelScreen::Replace: levelReplaceMenu(out,hits,level,width,height); break;
    case LevelScreen::Play:
    case LevelScreen::Transition:
    case LevelScreen::Complete: levelRunHud(out,hits,level,width,height); break;
    case LevelScreen::Error: levelErrorMenu(out,hits,level,width,height); break;
    case LevelScreen::Closed: break;
    }
}

SectionCanvas inventoryCanvas(int width, int height, float contentScale)
{
    if (!std::isfinite(contentScale) || contentScale <= 0) contentScale = 1;
    const float scale=std::min({std::clamp(contentScale,0.5f,4.0f),width/784.0f,height/530.0f});
    return {scale,width/scale,height/scale};
}

void oreIcon(Drawing& out, ItemId item, glm::vec2 center, float scale)
{
    const auto color=itemInfo(item).color;
    const auto p=[&](float x,float y) { return center+glm::vec2(x,y)*scale; };
    const auto facet=[&](glm::vec2 a,glm::vec2 b,glm::vec2 c,float shade) {
        out.triangle(center+a*scale,center+b*scale,center+c*scale,glm::vec4(color*shade,1));
    };
    if (item==ItemId::Key) {
        const glm::vec4 brass(color,1),light(glm::mix(color,glm::vec3(1),.35f),1);
        const std::array<glm::vec2,8> ring{{{-13,-14},{-5,-14},{0,-9},{0,-1},
            {-5,4},{-13,4},{-18,-1},{-18,-9}}};
        for (std::size_t i=0;i<ring.size();++i)
            out.bar(p(ring[i].x,ring[i].y),p(ring[(i+1)%ring.size()].x,ring[(i+1)%ring.size()].y),4*scale,brass);
        out.bar(p(-2,1),p(14,17),5*scale,brass);
        out.bar(p(7,10),p(13,4),5*scale,brass);
        out.bar(p(12,15),p(18,9),5*scale,brass);
        out.bar(p(-13,-15),p(-5,-15),1.5f*scale,light);
        out.bar(p(-2,0),p(13,15),1.3f*scale,light);
        return;
    }
    const int shape=static_cast<int>(item)%3;
    out.quad(center.x-15*scale,center.y+13*scale,31*scale,3*scale,{0,0,0,0.22f});
    if (shape==0) {
        // A jagged cluster with a pale cut face.
        facet({-17,7},{-9,-9},{2,3},.62f);
        facet({-9,-9},{1,-17},{2,3},1.23f);
        facet({1,-17},{13,-6},{2,3},.98f);
        facet({13,-6},{17,9},{2,3},.64f);
        facet({17,9},{2,16},{2,3},.82f);
        facet({2,16},{-17,7},{2,3},.48f);
        out.triangle(p(-9,-9),p(1,-17),p(-1,-11),glm::vec4(glm::mix(color,glm::vec3(1),.55f),1));
    } else if (shape==1) {
        // Rough metal ore, with exposed mineral seams on the dark host rock.
        constexpr glm::vec4 rock{.20f,.25f,.27f,1};
        out.triangle(p(-17,8),p(-11,-10),p(11,-13),rock);
        out.triangle(p(-17,8),p(11,-13),p(18,5),rock);
        out.triangle(p(-17,8),p(18,5),p(7,16),{.12f,.17f,.19f,1});
        facet({-11,-4},{-4,-10},{2,-4},1.15f);
        facet({-11,-4},{2,-4},{-4,3},.78f);
        facet({2,2},{12,-5},{15,5},1.0f);
        facet({2,2},{15,5},{7,11},.64f);
        out.bar(p(-12,6),p(-3,10),2*scale,glm::vec4(color,1));
    } else {
        // A pair of angular crystals.
        facet({-13,7},{-9,-11},{0,-1},.78f);
        facet({-9,-11},{-2,-6},{0,-1},1.20f);
        facet({-13,7},{0,-1},{-3,15},.50f);
        facet({-2,7},{6,-18},{10,-2},1.12f);
        facet({6,-18},{16,-9},{10,-2},1.35f);
        facet({10,-2},{16,-9},{15,10},.77f);
        facet({-2,7},{10,-2},{15,10},.88f);
        facet({-2,7},{15,10},{6,16},.54f);
    }
}

void packFrame(Drawing& out,float x,float y,float width,float height,glm::vec4 border,
               glm::vec4 fill,float corner=6)
{
    out.frame(x,y,width,height,border,fill,corner);
}

void inventoryUi(Drawing& out,std::vector<SectionHit>& hits,const Game& game,float width,float height)
{
    const auto& inventory=game.equipment.inventory;
    const bool chest=game.chestOpen();
    const float panelWidth=chest ? 748.0f : 396.0f;
    constexpr float panelHeight=482;
    const float x=(width-panelWidth)*.5f,y=(height-panelHeight)*.5f;
    const glm::vec4 leather{.34f,.25f,.17f,1}, brass{.58f,.42f,.24f,1}, lining{.22f,.16f,.11f,1};
    out.quad(0,0,width,height,{.025f,.016f,.009f,.73f});
    packFrame(out,x+6,y+8,panelWidth,panelHeight,{0,0,0,.4f},{0,0,0,.4f},13);
    packFrame(out,x,y,panelWidth,panelHeight,brass,{.08f,.045f,.026f,.99f},12);
    packFrame(out,x+8,y+8,panelWidth-16,panelHeight-16,leather,Canvas,8);
    // Leather grain and brass fasteners match the stitched field satchel.
    out.quad(x+17,y+17,panelWidth-34,37,lining);
    for (int i=0;i<46;++i) {
        const float gx=x+20+float((i*139)%static_cast<int>(panelWidth-44));
        const float gy=y+62+float((i*83)%364);
        out.quad(gx,gy,3+(i%4)*3,1,{.40f,.28f,.15f,.35f});
    }
    for (const auto corner:{glm::vec2(x+12,y+12),glm::vec2(x+panelWidth-15,y+12),
                            glm::vec2(x+12,y+panelHeight-15),glm::vec2(x+panelWidth-15,y+panelHeight-15)}) {
        out.quad(corner.x,corner.y,3,3,brass);
        out.quad(corner.x,corner.y,1,1,Gold);
    }
    out.text("FIELD SATCHEL",x+24,y+23,2,Cream);
    fittedText(out,game.devMode() ? "PLAYTEST / LOOT RESETS WHEN YOU LEAVE"
                                : "ORE AND EXPEDITION SUPPLIES",
               x+24,y+44,panelWidth-94,1,Muted);
    sectionButton(out,hits,x+panelWidth-50,y+16,28,26,"X",InventoryUiClose);
    out.text(std::to_string(inventory.occupiedSlots())+" / 20 SLOTS",x+24,y+65,1,Muted);
    sectionButton(out,hits,x+262,y+58,94,24,"S - SORT",InventoryUiSort);
    for (int slot=0;slot<Inventory::SlotCount;++slot) {
        const float sx=x+24+(slot%5)*68,sy=y+92+(slot/5)*68;
        const auto& stack=inventory.slots[slot];
        const bool selected=slot==game.inventorySelection();
        packFrame(out,sx,sy,60,60,selected ? Gold : leather,
                  selected ? Selected : Panel,4);
        out.quad(sx+7,sy+4,44,1,selected ? brass : leather);
        out.text(std::to_string(slot+1),sx+5,sy+5,1,{.54f,.43f,.30f,1});
        if (stack.count>0 && validItem(stack.item)) {
            oreIcon(out,stack.item,{sx+30,sy+29},.90f);
            const auto count=std::to_string(stack.count);
            const float countWidth=(count.size()*6-1)*out.fontPixel(1);
            out.quad(sx+54-countWidth,sy+45,countWidth+3,12,{.06f,.042f,.025f,.92f});
            out.text(count,sx+54-countWidth,sy+48,1,Cream);
        } else {
            out.quad(sx+27,sy+28,6,2,{.23f,.17f,.11f,1});
            out.quad(sx+29,sy+26,2,6,{.23f,.17f,.11f,1});
        }
        hits.push_back({sx,sy,60,60,InventoryUiSlotBase+slot});
    }
    packFrame(out,x+24,y+371,332,62,leather,{.13f,.12f,.084f,1},5);
    const int selected=game.inventorySelection();
    if (selected>=0 && selected<Inventory::SlotCount && inventory.slots[selected].count>0) {
        const auto& info=itemInfo(inventory.slots[selected].item);
        fittedText(out,info.name,x+35,y+381,205,1.25f,Leaf);
        fittedText(out,info.rarity,x+245,y+382,101,1,Gold);
        wrappedDescription(out,info.description,x+35,y+400,
                           static_cast<std::size_t>(310/(6*out.fontPixel(1))));
    } else {
        out.text("CLICK AN ITEM TO INSPECT OR MOVE IT",x+35,y+382,1,Muted);
        out.text("THEN CLICK A SLOT TO MOVE OR SWAP",x+35,y+401,1,Cream);
        out.text("TOOLS KEEP THEIR OWN HOTBAR",x+35,y+415,1,Muted);
    }
    if (chest) {
        const float cx=x+396;
        out.quad(cx-20,y+65,1,368,Edge);
        fittedText(out,game.chestName(),cx,y+65,302,1.25f,Gold);
        const auto& contents=game.chestContents();
        const bool roomy=contents.size()<=6;
        bool hasItems=false;
        for (std::size_t slot=0;slot<contents.size() && slot<ChestSlotCount;++slot) {
            const auto& stack=contents[slot];
            if (stack.count<=0 || !validItem(stack.item)) continue;
            hasItems=true;
            const float sx=cx+(slot%2)*153;
            const float sy=y+92+(slot/2)*(roomy ? 89 : 26);
            const float sw=145,sh=roomy ? 81 : 24;
            packFrame(out,sx,sy,sw,sh,brass,{.20f,.145f,.081f,1},4);
            oreIcon(out,stack.item,{sx+(roomy ? 45 : 14),sy+(roomy ? 34 : 12)},roomy ? 1.05f : .48f);
            fittedText(out,(roomy ? "X" : "")+std::to_string(stack.count),sx+(roomy ? 97 : 115),sy+(roomy ? 32 : 8),roomy ? 40 : 25,1,Cream);
            fittedText(out,itemInfo(stack.item).name,sx+(roomy ? 8 : 29),sy+(roomy ? 64 : 8),roomy ? sw-16 : 82,1,Muted);
            hits.push_back({sx,sy,sw,sh,InventoryUiLootBase+static_cast<int>(slot)});
        }
        if (!hasItems) {
            out.frame(cx,y+92,298,267,Edge);
            out.centered("CHEST EMPTY",cx+149,y+194,1.5f,Gold);
            out.centered("YOUR LOOT IS IN YOUR PACK",cx+149,y+219,1,Muted);
        }
        sectionButton(out,hits,cx,y+371,298,32,"ENTER - TAKE ALL",InventoryUiTakeAll,
                      hasItems,hasItems,1.25f);
        out.centered("CLICK A STACK TO TAKE WHAT FITS",cx+149,y+414,1,Muted);
    }
    out.quad(x+22,y+442,panelWidth-44,1,Edge);
    const auto& message=game.inventoryMessage();
    if (!message.empty()) fittedText(out,message,x+24,y+452,panelWidth-48,1,Leaf);
    else out.text("I / ESC CLOSE",x+24,y+453,1,Muted);
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
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture_);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_);
        glGetIntegerv(GL_SAMPLER_BINDING, &sampler_);
        glActiveTexture(static_cast<GLenum>(activeTexture_));
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
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture_));
        glBindSampler(0, static_cast<GLuint>(sampler_));
        glActiveTexture(static_cast<GLenum>(activeTexture_));
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
    GLint activeTexture_ = GL_TEXTURE0, texture_ = 0, sampler_ = 0;
};

void render(const Drawing& drawing, const Shader& shader, GLuint vao, GLuint buffer,
            PalettePreviews* previews = nullptr)
{
    if (drawing.vertices.empty()) return;
    const SavedState savedState;
    if (previews && !drawing.previews.empty()) {
        previews->ensure(drawing.previews);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, previews->texture());
        glBindSampler(0, 0);
    }
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    shader.use();
    if (previews) shader.setInt("previewAtlas", 0);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(drawing.vertices.size() * sizeof(Vertex)),
                 drawing.vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(drawing.vertices.size()));
}

} // namespace

Hud::Hud() : shader_(vertexShader, fragmentShader), previewShader_(vertexShader, previewFragmentShader)
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
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, uv)));
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
    drawing.frame(margin, margin, widest * advance + 2.0f * padding,
                  lines.size() * lineHeight - 2.0f * pixel + 2.0f * padding, Edge,Panel,3*pixel);
    for (std::size_t line = 0; line < lines.size(); ++line)
        drawing.text(lines[line],margin+padding,margin+padding+line*lineHeight,pixel,Cream);
    render(drawing, shader_, vao_, buffer_);
}

void Hud::drawMonsters(const Game& game, int framebufferWidth, int framebufferHeight, float contentScale)
{
    if (!game.monsterPreviewActive() || framebufferWidth <= 0 || framebufferHeight <= 0) return;
    if (!std::isfinite(contentScale) || contentScale <= 0) contentScale = 1;
    const float scale = std::min({std::clamp(contentScale,.5f,4.0f),
        framebufferWidth/660.0f,framebufferHeight/400.0f});
    const float width = framebufferWidth/scale, height = framebufferHeight/scale;
    Drawing drawing(width,height,scale);
    const float center = width*.5f;
    drawing.frame(center-237,16,474,53,Edge,Panel);
    drawing.centered("THE GROTTO / CREATURE STUDIES",center,27,1.5f,Cream);
    drawing.centered("SCRAPBONE   /   HOLLOW SAGE   /   MOSSDROP",center,51,1,Muted);
    constexpr std::array labels{"1  IDLE", "2  MOVE", "3  ATTACK", "E  AUTO"};
    const auto& preview = game.monsterPreview();
    for (int i=0; i<4; ++i) {
        const bool active = i==3 ? preview.isAutomatic()
            : static_cast<int>(preview.animation())==i;
        const float x = center-236+i*120;
        drawing.frame(x,height-87,112,30,active ? Gold : Edge,active ? Selected : Panel);
        drawing.centered(labels[i],x+56,height-77,1,active ? Gold : Cream);
    }
    drawing.frame(center-290,height-48,580,31,Edge,Panel);
    drawing.centered("WASD WALK / SPACE JUMP / R RESET / F3 OR B VILLAGE",center,height-39,1,Muted);
    drawing.centered("ANIMATION PREVIEW / COMBAT COMES LATER",center,height-25,.8f,Muted);
    render(drawing,shader_,vao_,buffer_);
}

void Hud::drawPlay(const Game& game, int framebufferWidth, int framebufferHeight, float contentScale)
{
    if (game.editorMode() || framebufferWidth <= 0 || framebufferHeight <= 0) return;
    if (!std::isfinite(contentScale) || contentScale <= 0) contentScale = 1;
    // Preserve the layout on small windows, then apply the display's DPI scale.
    const float minWidth=game.crossingMenuOpen() ? 620.0f : game.levelActive() ? 776.0f : 460.0f;
    const float minHeight=game.crossingMenuOpen() ? 470.0f : game.levelActive() ? 550.0f : 440.0f;
    const float scale = std::min({std::clamp(contentScale,0.5f,4.0f),
                                 framebufferWidth / minWidth, framebufferHeight / minHeight});
    const float width = framebufferWidth / scale, height = framebufferHeight / scale;
    Drawing drawing(width,height,scale);
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
    const float barTop = height - 64;
    toolBelt(drawing,equipment,center,barTop,needle);
    quietText(drawing,"H HELP",width-82,height-27,64,Muted);
    if (selected==Tool::Compass && !game.menuOpen()) {
        const auto distance=static_cast<int>(std::round(glm::length(target)));
        quietCentered(drawing,std::string(game.levelActive() ? "EXIT " : game.rootCrossingActive() ? "EXIT " : "HEART TREE ")
            +std::to_string(distance)+"M",center,barTop-17,240,Muted);
    }

    if (game.rootCrossingActive() && !game.crossingMenuOpen())
        crossingProgress(drawing,game,width,height);

    if (!game.menuOpen() && game.status.starts_with("Progress unavailable"))
        quietText(drawing,"SAVE DATA UNAVAILABLE / H DETAILS",18,height-66,width-36,Gold);
    if (game.nearBlacksmith() && !game.menuOpen()) {
        quietCentered(drawing,"E  BLACKSMITH",center,height*.5f+30,180,Gold);
    }
    if (game.nearExpeditionGate() && !game.menuOpen()) {
        quietCentered(drawing,"E  EXPEDITIONS",center,height*.5f+30,220,Leaf);
    }
    if (game.smithOpen()) {
        drawing.quad(0,0,width,height,{.025f,.016f,.009f,.55f});
        constexpr float panelWidth = 410, panelHeight = 300;
        const float x = center-panelWidth*0.5f, y = height*0.5f-panelHeight*0.5f-28;
        drawing.quad(x+5,y+6,panelWidth,panelHeight,{0,0,0,0.35f});
        drawing.frame(x,y,panelWidth,panelHeight,Edge);
        drawing.quad(x+12,y+12,panelWidth-24,43,Canvas);
        drawing.text("BLACKSMITH",x+22,y+19,2,Gold);
        drawing.text("THE VILLAGE FORGE",x+23,y+42,1,Muted);
        drawing.quad(x+20,y+61,panelWidth-40,1,Edge);
        drawing.frame(x+22,y+77,80,74,Edge,Canvas);
        pickaxeIcon(drawing,{x+61,y+115},1.75f,equipment.pickaxeTier);
        drawing.text("PICKAXE",x+122,y+81,1.75f,Cream);
        fittedText(drawing,"CURRENT TIER "+std::to_string(equipment.pickaxeTier),x+122,y+103,panelWidth-144,1.25f,Muted);
        const auto cost = pickaxeUpgradeCost(equipment.pickaxeTier);
        const bool affordable = cost && equipment.inventory.count(ItemId::CopperOre) >= cost->copper && equipment.inventory.count(ItemId::IronOre) >= cost->iron;
        fittedText(drawing,cost ? "NEXT TIER "+std::to_string(equipment.pickaxeTier+1) : "FULLY UPGRADED",
                   x+122,y+124,panelWidth-144,1.25f,Gold);
        std::string feedback = game.status;
        std::replace(feedback.begin(),feedback.end(),'\n',' ');
        std::replace(feedback.begin(),feedback.end(),'\r',' ');
        if (feedback.size() > 60) feedback = feedback.substr(0,57) + "...";
        const bool error = feedback.starts_with("Upgrade not saved") || feedback.starts_with("Progress unavailable")
            || feedback.starts_with("Not enough");
        fittedText(drawing,feedback,x+24,y+153,panelWidth-48,1,error ? glm::vec4(0.94f,0.53f,0.39f,1)
                   : feedback.starts_with("Pickaxe upgraded") ? Gold : Muted);
        drawing.text("UPGRADE COST",x+24,y+166,1,Muted);
        drawing.text("IN YOUR PACK",x+225,y+166,1,Muted);
        drawing.text(cost ? std::to_string(cost->copper)+" COPPER" : "-",x+24,y+183,1.25f,
                     cost && equipment.inventory.count(ItemId::CopperOre) < cost->copper ? glm::vec4(0.91f,0.47f,0.35f,1) : Cream);
        drawing.text(cost ? std::to_string(cost->iron)+" IRON" : "-",x+24,y+202,1.25f,
                     cost && equipment.inventory.count(ItemId::IronOre) < cost->iron ? glm::vec4(0.91f,0.47f,0.35f,1) : Cream);
        fittedText(drawing,std::to_string(equipment.inventory.count(ItemId::CopperOre))+" COPPER",x+225,y+183,panelWidth-249,1.25f,Cream);
        fittedText(drawing,std::to_string(equipment.inventory.count(ItemId::IronOre))+" IRON",x+225,y+202,panelWidth-249,1.25f,Cream);
        drawing.frame(x+22,y+226,panelWidth-44,30,affordable ? Gold : Edge,
                      affordable ? glm::vec4(0.77f,0.51f,0.18f,1) : Canvas);
        fittedCentered(drawing,!cost ? "MAXIMUM TIER" : affordable ? "ENTER - UPGRADE" : "MORE MATERIALS NEEDED",
                       center,y+236,panelWidth-64,1.5f,affordable ? Panel : Muted);
        drawing.centered("E / ESC - CLOSE",center,y+268,1, Cream);
        drawing.centered("STARTER GIFT: 12 COPPER",center,y+283,1,Muted);
    }
    if (game.crossingMenuOpen()) crossingPicker(drawing,game,width,height);
    render(drawing, shader_, vao_, buffer_);
}

void Hud::drawMining(const Game& game,int framebufferWidth,int framebufferHeight,float contentScale)
{
    if (!game.healthVisible() || game.health().dead() || game.equipment.selectedTool()!=Tool::Pickaxe
        || framebufferWidth<=0 || framebufferHeight<=0) return;
    const auto& mining=game.mining();
    const auto target=mining.target();
    const auto& message=game.miningMessage();
    if (!target && message.empty()) return;
    if (!std::isfinite(contentScale) || contentScale<=0) contentScale=1;
    const float scale=std::min({std::clamp(contentScale,.5f,4.0f),
        framebufferWidth/780.0f,framebufferHeight/550.0f});
    const float width=framebufferWidth/scale,height=framebufferHeight/scale;
    Drawing drawing(width,height,scale);
    const float center=width*.5f,top=height*.5f+26;
    constexpr float w=200;
    if (!message.empty()) {
        quietCentered(drawing,"MINING NOT SAVED",center,top,w,Gold);
        quietCentered(drawing,message,center,top+16,420,Cream);
    } else if (game.lootPrompt().empty() || mining.progress()>0) {
        quietCentered(drawing,blockName(target->block),center,top,w,Cream);
        if (mining.blocked()) {
            const auto ore=oreInfo(target->block);
            quietCentered(drawing,"TIER "+std::to_string(ore ? ore->requiredTier : 1)+" PICKAXE REQUIRED",
                center,top+16,w,{.95f,.58f,.39f,1});
        } else if (mining.progress()>0) {
            constexpr float meter=112;
            const float fill=meter*std::clamp(mining.progress(),0.0f,1.0f);
            drawing.quad(center-meter*.5f-1,top+15,meter+2,5,{0,0,0,.65f});
            drawing.quad(center-meter*.5f,top+16,fill,3,Gold);
        }
    }
    render(drawing,shader_,vao_,buffer_);
}

void Hud::drawCombat(const Game& game,int framebufferWidth,int framebufferHeight,float contentScale)
{
    if(!game.healthVisible() || game.health().dead() || framebufferWidth<=0 || framebufferHeight<=0) return;
    if(!std::isfinite(contentScale) || contentScale<=0) contentScale=1;
    const float scale=std::min({std::clamp(contentScale,.5f,4.0f),framebufferWidth/780.0f,framebufferHeight/440.0f});
    const float width=framebufferWidth/scale,height=framebufferHeight/scale;
    Drawing drawing(width,height,scale);
    const auto& encounter=game.encounters();
    const auto viewCamera=game.viewCamera();
    const auto projection=viewCamera.projectionMatrix(float(framebufferWidth)/framebufferHeight)*viewCamera.viewMatrix();
    const auto eye=game.camera.position();
    const auto collision=game.sectionSceneCollision();
    for(const auto& bar:encounter.healthBars()) {
        if(bar.visibleSeconds<=0 || bar.current<=0 || bar.maximum<=0) continue;
        const auto delta=bar.position-eye;
        const float distance=glm::length(delta);
        if(!std::isfinite(distance) || distance<.01f || distance>28) continue;
        const auto clip=projection*glm::vec4(bar.position,1);
        if(clip.w<=0) continue;
        const auto ndc=glm::vec3(clip)/clip.w;
        if(std::abs(ndc.x)>1 || ndc.y< -1 || ndc.z< -1 || ndc.z>1) continue;
        if(ndc.y>1) {
            const auto chest=projection*glm::vec4(bar.position-glm::vec3(0,1.5f,0),1);
            if(chest.w<=0 || chest.y/chest.w>1) continue;
        }
        // Do not reveal enemies through cave walls, stair geometry or props.
        if(raycast(eye,delta,distance-.08f,[&](glm::ivec3 point) {
            const auto block=game.world.get(point);
            return isSolid(block) ? block : Block::Air;
        })) continue;
        if(collision && collision->raycast(eye,delta,distance-.08f)) continue;
        const float x=(ndc.x*.5f+.5f)*width;
        // Leave room for the compact section label at the top of the view.
        const float minimumY=game.devMode() || game.levelActive() ? 55 : 20;
        const float y=std::max(minimumY,(.5f-ndc.y*.5f)*height);
        const float w=std::clamp(260.0f/distance,44.0f,88.0f);
        drawing.quad(x-w*.5f-1,y-1,w+2,6,{0,0,0,.7f});
        const float fill=w*std::clamp(float(bar.current)/bar.maximum,0.0f,1.0f);
        drawing.quad(x-w*.5f,y,w,4,{.22f,.10f,.075f,1});
        drawing.quad(x-w*.5f,y,fill,4,{.72f,.20f,.16f,1});
        drawing.quad(x-w*.5f,y,fill,1,{.98f,.49f,.33f,1});
    }
    if(!encounter.pendingCompletions().empty()) {
        quietCentered(drawing,"CLEAR NOT SAVED / RETRYING",width*.5f,58,440,Gold);
        quietCentered(drawing,game.inventoryMessage(),width*.5f,74,440,Cream);
    } else if(encounter.active()) {
        const auto remaining=encounter.remaining();
        const std::string text="TRIAL / "+std::to_string(remaining)+(remaining==1 ? " SPAWNER REMAINS" : " SPAWNERS REMAIN");
        quietText(drawing,text,width-250,20,232,Gold);
    }
    render(drawing,shader_,vao_,buffer_);
}

void Hud::drawHealth(const Game& game, int framebufferWidth, int framebufferHeight, float contentScale)
{
    if (!game.healthVisible() || framebufferWidth<=0 || framebufferHeight<=0) return;
    if (!std::isfinite(contentScale) || contentScale<=0) contentScale=1;
    // Leave room beside the central tool pockets, even in narrow windows.
    const float scale=std::min({std::clamp(contentScale,.5f,4.0f),
        framebufferWidth/780.0f,framebufferHeight/440.0f});
    const float width=framebufferWidth/scale,height=framebufferHeight/scale;
    Drawing drawing(width,height,scale);
    const auto& health=game.health();
    const float flash=health.dead() || game.menuOpen() ? 0 : std::clamp(health.damageFlash(),0.0f,1.0f);
    if (flash>0) {
        // Eight connected strips feather into clear space. Their vertex alpha
        // keeps the short pulse smooth at any resolution, without tinting aim.
        constexpr std::array<glm::vec2,8> rim{{{0,0},{.5f,0},{1,0},{1,.5f},
            {1,1},{.5f,1},{0,1},{0,.5f}}};
        auto forward=game.camera.forward(); forward.y=0;
        if (glm::dot(forward,forward)>.0001f) forward=glm::normalize(forward);
        else forward={0,0,-1};
        const auto right=glm::cross(forward,glm::vec3(0,1,0));
        auto source=-game.playerHitDirection(); source.y=0;
        glm::vec2 side(0);
        if (glm::dot(source,source)>.0001f) {
            source=glm::normalize(source);
            side={glm::dot(source,right),-glm::dot(source,forward)};
        }
        const auto point=[&](int index,float depth) {
            return glm::mix(rim[index],glm::vec2(.5f),depth*.36f)*glm::vec2(width,height);
        };
        const auto color=[&](int index,float depth) {
            const auto normal=glm::normalize(rim[index]-glm::vec2(.5f));
            const float emphasis=glm::dot(normal,side);
            const float fade=(1-depth)*(1-depth);
            return glm::vec4(.84f,.13f,.055f,flash*fade*(.46f+.20f*emphasis));
        };
        for (int band=0;band<8;++band) {
            const float outer=band/8.0f,inner=(band+1)/8.0f;
            for (int edge=0;edge<8;++edge) {
                const int next=(edge+1)%8;
                drawing.gradientQuad({point(edge,outer),point(next,outer),point(next,inner),point(edge,inner)},
                    {color(edge,outer),color(next,outer),color(next,inner),color(edge,inner)});
            }
        }
    }

    constexpr float x=18,w=144,h=28;
    const float y=height-44;
    const glm::vec4 blood{.73f,.20f,.17f,1};
    const glm::vec4 bright{.97f,.44f,.31f,1};
    drawing.quad(x-5,y-4,w+10,h+8,glm::mix(glm::vec4(.04f,.03f,.025f,.60f),
        glm::vec4(.24f,.09f,.055f,.80f),flash*.7f));
    constexpr std::array<std::uint8_t,6> heart{0x66,0xff,0xff,0x7e,0x3c,0x18};
    for (int row=0;row<6;++row) for (int column=0;column<8;++column)
        if (heart[row]&(1u<<(7-column)))
            drawing.quad(x+column*1.3f,y+16+row*1.3f,1.3f,1.3f,row<2 ? bright : blood);
    quietText(drawing,std::to_string(health.current()),x+22,y+3,w-22,
        health.current()<=25 ? bright : Cream);
    constexpr float meterWidth=w-22;
    drawing.quad(x+22,y+17,meterWidth,6,{.23f,.10f,.075f,1});
    const float fill=meterWidth*std::clamp(health.fraction(),0.0f,1.0f);
    if (fill>0) {
        drawing.quad(x+22,y+17,fill,6,glm::mix(blood,bright,flash*.45f));
        drawing.quad(x+22,y+17,fill,1,glm::mix(bright,Cream,flash*.55f));
    }

    if (health.dead()) {
        drawing.quad(0,0,width,height,{.035f,.018f,.018f,.67f});
        constexpr float panelWidth=350,panelHeight=130;
        const float center=width*.5f,top=height*.5f-panelHeight*.5f;
        drawing.frame(center-panelWidth*.5f+4,top+5,panelWidth,panelHeight,{0,0,0,.3f},{0,0,0,.3f});
        drawing.frame(center-panelWidth*.5f,top,panelWidth,panelHeight,Edge,Panel,9);
        drawing.centered("YOU DIED",center,top+23,2.5f,bright);
        fittedCentered(drawing,"RETURNING TO "+std::string(game.respawnDestination()),
            center,top+56,panelWidth-36,1.2f,Cream);
        constexpr float progressWidth=236;
        drawing.quad(center-progressWidth*.5f,top+80,progressWidth,4,Canvas);
        drawing.quad(center-progressWidth*.5f,top+80,
            progressWidth*std::clamp(health.respawnProgress(),0.0f,1.0f),4,Gold);
        drawing.centered("AUTOMATIC RESPAWN",center,top+101,1,Muted);
    }
    render(drawing,shader_,vao_,buffer_);
}

void Hud::drawSection(const Game& game,int framebufferWidth,int framebufferHeight,float contentScale)
{
    if (!game.devMode() || framebufferWidth<=0 || framebufferHeight<=0) return;
    const auto canvas=sectionCanvas(framebufferWidth,framebufferHeight,contentScale);
    Drawing drawing(canvas.width,canvas.height,canvas.scale);
    std::vector<SectionHit> hits;
    sectionUi(drawing,hits,game,canvas.width,canvas.height);
    if (drawing.previews.empty()) render(drawing,shader_,vao_,buffer_);
    else render(drawing,previewShader_,vao_,buffer_,&previews_);
}

std::optional<int> Hud::sectionClick(const Game& game,int framebufferWidth,int framebufferHeight,
                                     float contentScale,float x,float y)
{
    if (!game.devMode() || !game.sectionEditor().menuOpen() || framebufferWidth<=0 || framebufferHeight<=0
        || !std::isfinite(x) || !std::isfinite(y)) return std::nullopt;
    const auto canvas=sectionCanvas(framebufferWidth,framebufferHeight,contentScale);
    Drawing drawing(canvas.width,canvas.height,canvas.scale);
    std::vector<SectionHit> hits;
    sectionUi(drawing,hits,game,canvas.width,canvas.height);
    x/=canvas.scale; y/=canvas.scale;
    for (const auto& hit:hits)
        if (x>=hit.x && x<hit.x+hit.width && y>=hit.y && y<hit.y+hit.height) return hit.action;
    return std::nullopt;
}

void Hud::drawLevels(const Game& game,int framebufferWidth,int framebufferHeight,float contentScale)
{
    if (!game.levelController().open() || framebufferWidth<=0 || framebufferHeight<=0) return;
    const auto canvas=levelCanvas(framebufferWidth,framebufferHeight,contentScale);
    Drawing drawing(canvas.width,canvas.height,canvas.scale);
    std::vector<SectionHit> hits;
    levelUi(drawing,hits,game,canvas.width,canvas.height);
    render(drawing,shader_,vao_,buffer_);
}

std::optional<int> Hud::levelClick(const Game& game,int framebufferWidth,int framebufferHeight,
                                   float contentScale,float x,float y)
{
    if (!game.levelController().menuOpen() || framebufferWidth<=0 || framebufferHeight<=0
        || !std::isfinite(x) || !std::isfinite(y)) return std::nullopt;
    const auto canvas=levelCanvas(framebufferWidth,framebufferHeight,contentScale);
    Drawing drawing(canvas.width,canvas.height,canvas.scale);
    std::vector<SectionHit> hits;
    levelUi(drawing,hits,game,canvas.width,canvas.height);
    x/=canvas.scale; y/=canvas.scale;
    for (const auto& hit:hits)
        if (x>=hit.x && x<hit.x+hit.width && y>=hit.y && y<hit.y+hit.height) return hit.action;
    return std::nullopt;
}

void Hud::drawInventory(const Game& game,int framebufferWidth,int framebufferHeight,float contentScale)
{
    if (game.editorMode() || framebufferWidth<=0 || framebufferHeight<=0
        || (game.devMode() && game.sectionEditor().screen()!=SectionEditorScreen::Test)) return;
    const auto canvas=inventoryCanvas(framebufferWidth,framebufferHeight,contentScale);
    Drawing drawing(canvas.width,canvas.height,canvas.scale);
    std::vector<SectionHit> hits;
    if (game.inventoryOpen()) inventoryUi(drawing,hits,game,canvas.width,canvas.height);
    else if (!game.menuOpen()) {
        if (game.equipment.inventory.occupiedSlots()==Inventory::SlotCount)
            quietText(drawing,"PACK 20/20",canvas.width-90,canvas.height-46,76,Gold);
        if (!game.lootPrompt().empty() && game.mining().progress()==0) {
            quietCentered(drawing,game.lootPrompt(),canvas.width*.5f,canvas.height*.5f+30,
                canvas.width-36,Gold);
        }
    }
    render(drawing,shader_,vao_,buffer_);
}

std::optional<int> Hud::inventoryClick(const Game& game,int framebufferWidth,int framebufferHeight,
                                      float contentScale,float x,float y)
{
    if (!game.inventoryOpen() || framebufferWidth<=0 || framebufferHeight<=0
        || !std::isfinite(x) || !std::isfinite(y)) return std::nullopt;
    const auto canvas=inventoryCanvas(framebufferWidth,framebufferHeight,contentScale);
    Drawing drawing(canvas.width,canvas.height,canvas.scale);
    std::vector<SectionHit> hits;
    inventoryUi(drawing,hits,game,canvas.width,canvas.height);
    x/=canvas.scale; y/=canvas.scale;
    for (const auto& hit:hits)
        if (x>=hit.x && x<hit.x+hit.width && y>=hit.y && y<hit.y+hit.height) return hit.action;
    return std::nullopt;
}

} // namespace voxel
