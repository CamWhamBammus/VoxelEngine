#pragma once

#include "graphics/journal_font_data.h"
#include <algorithm>
#include <string_view>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace voxel::journal {

inline const font::Glyph& letter(char c)
{
    const auto code=static_cast<unsigned char>(c);
    return font::Glyphs[code>=32 && code<=126 ? code-32 : '?' - 32];
}

inline float textWidth(std::string_view text,float height)
{
    float width=0;
    for(const char c:text)width+=letter(c).advance*height/font::CapHeight;
    return width;
}

// Glyphs are ordinary HUD triangles, so every platform renders the same serif
// face and no texture, font loader, operating-system font or extra library is
// needed at runtime. Font copyright is retained beside the native glyph data.
template<class Drawing>
void text(Drawing& out,std::string_view value,float x,float y,float height,glm::vec4 ink,bool bold=false)
{
    const float unit=height/font::CapHeight;
    for(const char c:value) {
        const auto& glyph=letter(c);
        const auto point=[&](std::size_t i) {
            const auto p=font::Points[i];
            return glm::vec2(x+p[0]*unit,y+p[1]*unit);
        };
        for(std::size_t i=glyph.first;i<glyph.first+glyph.count;i+=3) {
            out.triangle(point(i),point(i+1),point(i+2),ink);
            if(bold) {
                const glm::vec2 offset(height*.014f,0);
                out.triangle(point(i)+offset,point(i+1)+offset,point(i+2)+offset,ink);
            }
        }
        x+=glyph.advance*unit;
    }
}

template<class Drawing>
void centered(Drawing& out, std::string_view value, float x, float y, float height, glm::vec4 ink, bool bold=false)
{
    text(out, value, x - textWidth(value,height)*.5f, y, height, ink,bold);
}

template<class Drawing>
void fitted(Drawing& out, std::string_view value, float x, float y, float width,
            float height, glm::vec4 ink, bool center = false, bool bold=false)
{
    const float requested = textWidth(value,height);
    if (requested > width) height *= width/requested;
    if (center) centered(out,value,x,y,height,ink,bold);
    else text(out,value,x,y,height,ink,bold);
}

template<class Drawing>
void paragraph(Drawing& out, std::string_view value, float center, float y, float width,
               float height, glm::vec4 ink)
{
    while (!value.empty()) {
        std::size_t end = value.size();
        if (textWidth(value,height) > width) {
            end = value.find(' ');
            while (end != std::string_view::npos) {
                const auto next = value.find(' ',end+1);
                if (next == std::string_view::npos || textWidth(value.substr(0,next),height)>width) break;
                end = next;
            }
            if (end == std::string_view::npos) end = value.size();
        }
        centered(out,value.substr(0,end),center,y,height,ink);
        y += height*1.65f;
        value.remove_prefix(std::min(value.size(),end+1));
    }
}

} // namespace voxel::journal
