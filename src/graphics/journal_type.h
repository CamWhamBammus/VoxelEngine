#pragma once

#include <algorithm>
#include <cmath>
#include <string_view>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace voxel {
namespace journal {

// Hand-drawn letter strokes for the journal, expressed on a ten-unit cap
// height. Keeping this tiny face native makes the book readable without a
// font loader, platform font, atlas texture or an extra runtime dependency.
struct Letter { std::string_view strokes; float advance; };
inline Letter letter(char c)
{
    switch (c) {
    case 'A': return {"0A408A/2666",9};
    case 'B': return {"0A00006081726303/037385887A0A",9};
    case 'C': return {"817060301001091A3A6A89",9};
    case 'D': return {"0A00006081893A0A",9};
    case 'E': return {"80000A8A/0545",8.5f};
    case 'F': return {"0A0080/0545",8};
    case 'G': return {"817060301001091A3A6A898557",9};
    case 'H': return {"000A/808A/0585",9};
    case 'I': return {"0040/202A/0A4A",5};
    case 'J': return {"4080796A3A1908",9};
    case 'K': return {"000A/80058A",9};
    case 'L': return {"000A8A89",8};
    case 'M': return {"0A0046808A",10};
    case 'N': return {"0A008A80",10};
    case 'O': return {"30608081897A3A1A09010030",10};
    case 'P': return {"0A006081827303",9};
    case 'Q': return {"30608081897A3A1A09010030/578B",10};
    case 'R': return {"0A006081827303/438A",9};
    case 'S': return {"81706030100112265778897A3A1909",9};
    case 'T': return {"0080/404A",9};
    case 'U': return {"0008193A6A798880",9};
    case 'V': return {"004A80",9};
    case 'W': return {"002A456A80",10};
    case 'X': return {"008A/800A",9};
    case 'Y': return {"004580/454A",9};
    case 'Z': return {"00800A8A",9};
    case 'a': return {"142353646A/64653616362A4A696A",7};
    case 'b': return {"000A/0323536468593A1A09",7};
    case 'c': return {"6453231418193A5A69",7};
    case 'd': return {"606A/6353231418193A5A69",7};
    case 'e': return {"166653231418193A5A69",7};
    case 'f': return {"2A216070/0444",5.5f};
    case 'g': return {"6353231418193A5A69636B5D2D0C",7};
    case 'h': return {"000A/032353646A",7.5f};
    case 'i': return {"2122/242A",4};
    case 'j': return {"3132/343B2D0D",4};
    case 'k': return {"000A/63176A",7};
    case 'l': return {"202A",4};
    case 'm': return {"030A/0323434A/436373848A",9.5f};
    case 'n': return {"030A/032353646A",7.5f};
    case 'o': return {"23536468593A1A09041323",7.5f};
    case 'p': return {"030D/0323536468593A1A09",7};
    case 'q': return {"636D/6353231418193A5A69",7};
    case 'r': return {"030A/03234364",6};
    case 's': return {"6453231415254657695A2A19",7};
    case 't': return {"21293A5A/0444",5.5f};
    case 'u': return {"0308193A5963636A",7.5f};
    case 'v': return {"033A63",7};
    case 'w': return {"031A454A73",8};
    case 'x': return {"036A/630A",7};
    case 'y': return {"033A/633C2D0D",7};
    case 'z': return {"03630A6A",7};
    case '0': return {"306081897A3A19010030",9};
    case '1': return {"12404A/1A7A",8};
    case '2': return {"012030608182730A8A",9};
    case '3': return {"01206081826343/637485887A3A1909",9};
    case '4': return {"6A600686",9};
    case '5': return {"800004645575887A3A1909",9};
    case '6': return {"80702001091A6A798775531304",9};
    case '7': return {"00802A",9};
    case '8': return {"30608183745575878A3A1907153413010030",9};
    case '9': return {"867453131001306081897A3A",9};
    case '.': return {"192A",3};
    case ',': return {"192A1C",3};
    case ':': return {"1415/191A",3};
    case '-': return {"1646",6};
    case '/': return {"600A",7};
    case '&': return {"797A3A1907157341201001128A/8575",10};
    case '!': return {"2026/292A",4};
    case '?': return {"0120608182634446/494A",9};
    case '\'': return {"2022",3};
    case ' ': return {"",4};
    default: return {"",6};
    }
}

inline float textWidth(std::string_view text, float height)
{
    float width = 0;
    for (const char c : text) width += letter(c).advance * height / 10;
    return width;
}

// Drawing only needs triangle() and bar(); this keeps the typeface separate
// from HUD state and usable by any future book or sign panel.
template<class Drawing>
void text(Drawing& out, std::string_view value, float x, float y, float height, glm::vec4 ink)
{
    const float unit = height / 10;
    const float weight = .72f * unit;
    const auto digit=[](char c) { return c <= '9' ? c - '0' : c - 'A' + 10; };
    for (const char c : value) {
        const auto face = letter(c);
        auto path = face.strokes;
        while (!path.empty()) {
            const auto end = path.find('/');
            const auto stroke = path.substr(0, end);
            const auto point = [&](std::size_t i) {
                return glm::vec2(x + digit(stroke[i]) * unit, y + digit(stroke[i+1]) * unit);
            };
            for (std::size_t i = 2; i + 1 < stroke.size(); i += 2)
                if (point(i-2) != point(i)) out.bar(point(i-2), point(i), weight, ink);
            if (stroke.size() >= 4) {
                // Small bracketed serifs flare the ends of vertical strokes.
                for (const bool last : {false, true}) {
                    const auto a = point(last ? stroke.size()-2 : 0);
                    const auto b = point(last ? stroke.size()-4 : 2);
                    if (std::abs(a.x-b.x) < unit*.25f && std::abs(a.y-b.y) > unit*1.5f) {
                        const float inset = a.y < b.y ? unit*.8f : -unit*.8f;
                        out.triangle(a+glm::vec2(-1.2f*unit,0),a+glm::vec2(1.2f*unit,0),
                                     a+glm::vec2(0,inset),ink);
                    }
                }
            }
            if (end == std::string_view::npos) break;
            path.remove_prefix(end+1);
        }
        x += face.advance * unit;
    }
}

template<class Drawing>
void centered(Drawing& out, std::string_view value, float x, float y, float height, glm::vec4 ink)
{
    text(out, value, x - textWidth(value,height)*.5f, y, height, ink);
}

template<class Drawing>
void fitted(Drawing& out, std::string_view value, float x, float y, float width,
            float height, glm::vec4 ink, bool center = false)
{
    const float requested = textWidth(value,height);
    if (requested > width) height *= width/requested;
    if (center) centered(out,value,x,y,height,ink);
    else text(out,value,x,y,height,ink);
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

} // namespace journal
} // namespace voxel
