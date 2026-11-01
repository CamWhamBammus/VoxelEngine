#pragma once

#include "graphics/journal_art.h"
#include "graphics/journal_layout.h"
#include "graphics/journal_type.h"
#include "skills.h"
#include <algorithm>
#include <array>
#include <cstdint>

namespace voxel::journal {

inline constexpr glm::vec4 Ink{.20f,.16f,.09f,1};
inline constexpr glm::vec4 FaintInk{.42f,.39f,.23f,.73f};
inline constexpr glm::vec4 MossInk{.25f,.34f,.13f,1};

template<class Drawing>
void foliageCorner(Drawing& out,float x,float y,float directionX,float directionY,float size=1)
{
    const auto p=[&](float px,float py){return glm::vec2(x+px*directionX*size,y+py*directionY*size);};
    out.bar(p(0,9),p(0,0),.85f*size,FaintInk);
    out.bar(p(0,0),p(10,0),.85f*size,FaintInk);
    out.triangle(p(2,3),p(6,3),p(4,8),FaintInk);
    out.triangle(p(5,2),p(10,3),p(7,6),FaintInk);
}

template<class Drawing>
void sprig(Drawing& out,float x,float y,float size=1)
{
    Illustration<Drawing> art(out,{x,y},size*76);
    art.line({-8,12},{6,-8},1,{.34f,.39f,.19f,.85f});
    art.leaf({-3,5},{-10,-3},3.4f,{.39f,.43f,.22f});
    art.leaf({1,-1},{9,-6},3.1f,{.41f,.46f,.24f});
    art.leaf({4,-5},{3,-13},2.6f,{.43f,.47f,.24f});
}

template<class Drawing>
void paperGrain(Drawing& out,float x,float y,float width,float height,std::uint32_t seed)
{
    const auto random=[&]() {seed=seed*1664525u+1013904223u;return (seed>>8)/16777216.f;};
    // A fixed seed stops the paper surface from crawling while browsing.
    for(int i=0;i<760;++i) {
        const float px=x+random()*width,py=y+random()*height;
        const float w=1+random()*6,h=.6f+random()*3;
        out.quad(px,py,std::min(w,x+width-px),std::min(h,y+height-py),
                 i%3 ? glm::vec4(.50f,.35f,.15f,.035f) : glm::vec4(1,.95f,.77f,.08f));
    }
}

template<class Drawing>
void bookPage(Drawing& out,float x,float y,bool right)
{
    const float left=x+(right?540:57),top=y+94,width=right?474:483,height=491;
    // The slight bend at the binding and irregular stepped outside edge give
    // the spread depth while keeping all text on an uncluttered paper field.
    for(int layer=5;layer>=0;--layer) {
        const float shift=layer*1.6f;
        const glm::vec4 paper(.72f+layer*.012f,.56f+layer*.013f,.34f+layer*.014f,1);
        out.frame(left-shift,top+shift,width+shift,height+shift,paper,paper,5);
    }
    out.gradientQuad({{{left,top},{left+width,top},{left+width,top+height},{left,top+height}}},
        {{{.89f,.78f,.57f,1},{.91f,.82f,.63f,1},{.85f,.72f,.51f,1},{.88f,.75f,.53f,1}}});
    out.gradientQuad({{{left+8,top+10},{left+width-8,top+10},{left+width-8,top+height-10},{left+8,top+height-10}}},
        {{{.93f,.83f,.65f,1},{.94f,.85f,.67f,1},{.92f,.81f,.60f,1},{.92f,.81f,.60f,1}}});
    paperGrain(out,left+9,top+11,width-18,height-23,right?1955:1207);
    // Warm worn edges remain subtle behind the skill artwork.
    for(int i=0;i<6;++i) {
        const float inset=3+i*2;
        out.quad(left+inset,top+inset,width-inset*2,1.2f,{.43f,.27f,.10f,.06f});
        out.quad(left+inset,top+height-inset,width-inset*2,1.2f,{.42f,.26f,.10f,.09f});
    }
    const float side=right?left+width-2:left+2;
    for(int i=0;i<16;++i) {
        const float py=top+17+i*29;
        out.quad(side-(right?3:0),py,3,8+(i%3)*3,{.52f,.34f,.14f,.15f});
    }
}

template<class Drawing>
void tabMark(Drawing& out,int tab,glm::vec2 c,float size)
{
    const auto p=[&](float x,float y){return c+glm::vec2(x,y)*size;};
    const glm::vec4 color{.82f,.74f,.53f,1};
    const auto line=[&](float x,float y,float x2,float y2,float w=1.7f){out.bar(p(x,y),p(x2,y2),w*size,color);};
    if(tab==0) {
        constexpr std::array<glm::vec2,7> shield{{{-9,-10},{0,-13},{9,-10},{8,3},{5,9},{0,13},{-7,7}}};
        for(std::size_t i=0;i<shield.size();++i)out.bar(p(shield[i].x,shield[i].y),p(shield[(i+1)%shield.size()].x,shield[(i+1)%shield.size()].y),1.5f*size,color);
        line(0,-9,0,8,1);
    } else if(tab==1) {
        line(-8,12,7,-9,3);line(-10,-7,-5,-12,3);line(-5,-12,5,-12,3);line(5,-12,12,-5,3);
    } else if(tab==2) {
        line(-10,11,10,-11,2.8f);line(10,11,-10,-11,2.8f);line(-11,4,-3,12,2);line(3,12,11,4,2);
    } else {
        for(int i=0;i<12;++i) {
            const float a=i*.523599f,b=(i+1)*.523599f;
            out.bar(p(std::cos(a)*11,std::sin(a)*11),p(std::cos(b)*11,std::sin(b)*11),1.2f*size,color);
        }
        line(-15,0,15,0,1);line(0,-15,0,15,1);
        out.triangle(p(-4,4),p(6,-9),p(3,3),color);
    }
}

template<class Drawing>
void drawBook(Drawing& out,const JournalLayout& layout,int currentTab,int selected)
{
    const float x=layout.x,y=layout.y;
    out.quad(0,0,layout.width,layout.height,{.018f,.025f,.016f,.62f});
    const auto book=layout.book();
    for(int shadow=8;shadow>0;--shadow)
        out.frame(book.x-shadow*2,book.y+4-shadow,book.width+shadow*4,book.height+shadow*3,
                  {.025f,.018f,.009f,.035f},{.025f,.018f,.009f,.035f},6);
    out.frame(book.x-2,book.y-5,book.width+4,book.height+7,{.10f,.065f,.032f,1},{.19f,.115f,.061f,1},9);
    out.frame(book.x+4,book.y,book.width-8,book.height-3,{.47f,.31f,.15f,1},{.24f,.15f,.075f,1},7);
    out.frame(book.x+9,book.y+5,book.width-18,book.height-13,{.11f,.07f,.036f,1},{.27f,.17f,.085f,1},7);
    for(int stitch=0;stitch<115;++stitch) {
        const float sx=book.x+13+stitch*8.7f;
        out.quad(sx,book.y+8,3,1,{.64f,.43f,.22f,.65f});
        out.quad(sx,book.y+book.height-13,3,1,{.64f,.43f,.22f,.65f});
    }
    for(int stitch=0;stitch<58;++stitch) {
        const float sy=book.y+16+stitch*8.7f;
        out.quad(book.x+10,sy,1,3,{.64f,.43f,.22f,.65f});
        out.quad(book.x+book.width-11,sy,1,3,{.64f,.43f,.22f,.65f});
    }
    const auto tabs=skillTabNames();
    for(int index=0;index<int(tabs.size());++index) {
        const auto tab=layout.tab(index);const bool active=index==currentTab;
        out.frame(tab.x-2,tab.y-2,tab.width+4,tab.height+5,{.11f,.08f,.04f,1},{.18f,.11f,.06f,1},6);
        out.frame(tab.x,tab.y,tab.width,tab.height,active?glm::vec4(.64f,.47f,.22f,1):glm::vec4(.44f,.28f,.14f,1),
                  active?glm::vec4(.30f,.36f,.16f,1):glm::vec4(.26f,.17f,.10f,1),5);
        for(int dash=0;dash<21;++dash)out.quad(tab.x+10+dash*8.5f,tab.y+5,3,1,{.68f,.48f,.25f,.5f});
        paperGrain(out,tab.x+7,tab.y+9,tab.width-14,tab.height-13,310+index);
        tabMark(out,index,{tab.x+34,tab.y+28},1.08f);
        fitted(out,tabs[index],tab.x+61,tab.y+19,tab.width-69,21,{.89f,.81f,.61f,1});
    }
    bookPage(out,x,y,false);bookPage(out,x,y,true);
    // The gutter is a sequence of translucent gradients instead of a black
    // divider: light paper rolls down into a warm leather crease.
    out.gradientQuad({{{x+518,y+96},{x+541,y+99},{x+541,y+585},{x+518,y+579}}},
        {{{.45f,.28f,.10f,0},{.34f,.19f,.065f,.48f},{.34f,.19f,.065f,.49f},{.45f,.28f,.10f,0}}});
    out.gradientQuad({{{x+541,y+99},{x+561,y+95},{x+561,y+580},{x+541,y+585}}},
        {{{.28f,.15f,.045f,.40f},{.57f,.38f,.17f,0},{.57f,.38f,.17f,0},{.28f,.15f,.045f,.43f}}});
    out.bar({x+541,y+100},{x+541,y+584},1.1f,{.36f,.21f,.07f,.5f});
    for(int ring=0;ring<3;++ring) {
        const float ry=y+145+ring*181;
        out.frame(x+537,ry,6,8,{.31f,.18f,.07f,1},{.59f,.40f,.17f,1},1);
    }
    text(out,"SKILL TREE",x+86,y+125,32,Ink);
    sprig(out,x+328,y+148,.9f);
    out.bar({x+85,y+169},{x+503,y+169},1,FaintInk);

    const auto entries=skillEntries(currentTab);
    for(int index=0;index<int(entries.size());++index) {
        const auto card=layout.card(index);const bool active=index==selected;
        const glm::vec4 outline=active?MossInk:glm::vec4(.42f,.34f,.20f,.35f);
        out.frame(card.x,card.y,card.width,card.height,outline,
                  active?glm::vec4(.51f,.60f,.26f,.20f):glm::vec4(.92f,.82f,.60f,.01f),3);
        if(active) {
            out.frame(card.x+2.5f,card.y+2.5f,card.width-5,card.height-5,{.34f,.43f,.16f,.66f},{.6f,.7f,.3f,.025f},2);
            out.bar({card.x+7,card.y+1},{card.x+card.width-8,card.y+1},2,MossInk);
            out.bar({card.x+6,card.y+card.height-2},{card.x+card.width-7,card.y+card.height-2},1.8f,MossInk);
        }
        foliageCorner(out,card.x+6,card.y+6,1,1,.73f);
        foliageCorner(out,card.x+card.width-6,card.y+6,-1,1,.73f);
        foliageCorner(out,card.x+6,card.y+card.height-6,1,-1,.73f);
        foliageCorner(out,card.x+card.width-6,card.y+card.height-6,-1,-1,.73f);
        icon(out,entries[index].icon,{card.x+card.width*.5f,card.y+83},105);
        fitted(out,entries[index].name,card.x+card.width*.5f,card.y+139,card.width-17,16,Ink,true);
    }
    // Facing page keeps one illustration and one short sentence, with no
    // progression widgets until the skill system itself has been designed.
    const float detailCenter=x+787;
    out.bar({x+574,y+127},{x+996,y+127},.8f,FaintInk);
    out.bar({x+574,y+567},{x+996,y+567},.8f,FaintInk);
    foliageCorner(out,x+573,y+126,1,1,1.1f);foliageCorner(out,x+998,y+126,-1,1,1.1f);
    foliageCorner(out,x+573,y+568,1,-1,1.1f);foliageCorner(out,x+998,y+568,-1,-1,1.1f);
    if(!entries.empty()) {
        const auto& entry=entries[std::clamp(selected,0,int(entries.size())-1)];
        icon(out,entry.icon,{detailCenter,y+326},290);
        sprig(out,detailCenter,y+442,.72f);
        out.bar({x+611,y+448},{x+969,y+448},.8f,FaintInk);
        fitted(out,entry.name,detailCenter,y+468,386,27,Ink,true);
        paragraph(out,entry.description,detailCenter,y+512,370,16,Ink);
    }
    text(out,"Preview only",x+891,y+550,11,{.43f,.37f,.24f,1});
    const auto close=layout.close();
    out.frame(close.x,close.y,38,27,{.61f,.57f,.45f,.85f},{.06f,.055f,.04f,.82f},2);
    centered(out,"Esc",close.x+19,close.y+7,12,{.87f,.82f,.68f,1});
    text(out,"Close",close.x+49,close.y+5,18,{.87f,.82f,.68f,1});
}

} // namespace voxel::journal
