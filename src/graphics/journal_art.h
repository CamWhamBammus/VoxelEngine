#pragma once

#include "skills.h"
#include <array>
#include <cmath>
#include <initializer_list>
#include <glm/glm.hpp>

namespace voxel::journal {

// Small native illustrations use the same chunky facets as the world. They
// scale from a card to the facing page without blurry raster enlargement.
template<class Drawing>
class Illustration {
public:
    Illustration(Drawing& drawing,glm::vec2 center,float size)
        : out(drawing),center(center),scale(size/76) {}
    void polygon(std::initializer_list<glm::vec2> points,glm::vec4 color) {
        if (points.size()<3) return;
        const auto start=*points.begin();
        for(auto at=points.begin()+1;at+1!=points.end();++at)
            out.triangle(p(start),p(*at),p(*(at+1)),color);
    }
    void line(glm::vec2 a,glm::vec2 b,float width,glm::vec4 color) {
        if(a!=b)out.bar(p(a),p(b),width*scale,color);
    }
    void cube(float x,float z,float y,float w,float d,float h,glm::vec3 color) {
        const auto v=[](float x,float z,float y){return glm::vec2((x-z)*.74f,(x+z)*.32f-y);};
        polygon({v(x,z,y+h),v(x+w,z,y+h),v(x+w,z+d,y+h),v(x,z+d,y+h)},glm::vec4(glm::mix(color,glm::vec3(1),.19f),1));
        polygon({v(x,z+d,y+h),v(x+w,z+d,y+h),v(x+w,z+d,y),v(x,z+d,y)},glm::vec4(color,1));
        polygon({v(x+w,z+d,y+h),v(x+w,z,y+h),v(x+w,z,y),v(x+w,z+d,y)},glm::vec4(color*.63f,1));
    }
    void leaf(glm::vec2 a,glm::vec2 b,float breadth,glm::vec3 color) {
        const auto axis=b-a,side=glm::normalize(glm::vec2(-axis.y,axis.x))*breadth;
        const auto middle=a+axis*.48f;
        polygon({a,middle+side,b},glm::vec4(color,1));
        polygon({a,b,middle-side},glm::vec4(color*.67f,1));
    }
    void shadow() {
        polygon({{-31,17},{-13,10},{26,13},{36,21},{20,28},{-17,28},{-34,23}}, {.27f,.29f,.12f,.15f});
        polygon({{-25,18},{-7,13},{23,17},{29,23},{9,27},{-23,24}}, {.25f,.26f,.11f,.12f});
    }
    void boots() {
        shadow();
        const auto boot=[&](float x,float z) {
            cube(x-10,z,0,20,17,4,{.22f,.14f,.085f});
            cube(x-10,z,4,20,17,8,{.45f,.26f,.12f});
            cube(x+1,z,12,9,15,25,{.34f,.19f,.095f});
            cube(x+1,z,35,9,15,3,{.55f,.34f,.16f});
            cube(x+2,z+1,38,7,13,.2f,{.09f,.065f,.035f});
            cube(x-9,z+1,11,10,15,1.4f,{.59f,.38f,.19f});
            cube(x-6,z+16.9f,6,4,.5f,3,{.36f,.19f,.09f});
            cube(x+1,z+14.8f,16,9,.6f,2,{.63f,.43f,.21f});
            cube(x+1,z+14.8f,27,9,.6f,2,{.64f,.43f,.21f});
            cube(x+4,z+15,16,3,.8f,3,{.72f,.57f,.27f});
            cube(x+4,z+15,27,3,.8f,3,{.72f,.57f,.27f});
            for(int i=0;i<5;++i) {
                const float height=9+i*5;
                cube(x+1+(i%3)*2,z+15.4f,height,3.5f,.8f,4,{.34f+.025f*i,.43f+.025f*i,.12f});
            }
            cube(x-7,z+16,11,4,1.3f,3,{.32f,.44f,.10f});
        };
        boot(3,-18);boot(-13,4);
        leaf({-16,0},{-25,-9},3,{.40f,.51f,.13f});
        leaf({-17,-1},{-8,-12},3,{.48f,.57f,.17f});
    }
    void chestplate() {
        shadow();
        polygon({{-27,-15},{-17,-23},{-9,-20},{-5,-11},{-16,0},{-28,-2}}, {.36f,.36f,.28f,1});
        polygon({{15,-23},{29,-16},{28,-2},{17,1},{5,-11},{9,-21}}, {.31f,.30f,.23f,1});
        polygon({{-16,-22},{-8,-26},{-5,-17},{7,-17},{10,-26},{19,-21},{17,15},{0,25},{-18,15}}, {.58f,.57f,.45f,1});
        polygon({{1,-14},{15,-20},{14,13},{0,20}}, {.35f,.36f,.29f,1});
        polygon({{-16,-19},{-9,-22},{-5,-13},{-2,-12},{-2,17},{-14,11}}, {.69f,.67f,.55f,1});
        polygon({{-6,-24},{8,-24},{5,-18},{-3,-18}}, {.18f,.19f,.14f,1});
        line({-18,11},{0,18},4,{.34f,.25f,.11f,1});line({0,18},{17,11},4,{.25f,.20f,.10f,1});
        cube(-5,-4,-13,7,4,4,{.65f,.48f,.19f});
        leaf({-16,12},{-25,4},3,{.33f,.43f,.14f});leaf({11,-4},{20,-10},4,{.42f,.49f,.15f});
    }
    void feather() {
        shadow();
        polygon({{-19,23},{-19,4},{-10,-15},{8,-31},{21,-35},{26,-27},{22,-8},{8,9}}, {.57f,.43f,.29f,1});
        polygon({{-18,18},{-12,-5},{-2,-21},{18,-33},{12,-13},{0,8}}, {.77f,.66f,.46f,1});
        polygon({{-18,18},{0,8},{12,-13},{18,-33},{23,-24},{16,-6},{4,12}}, {.42f,.30f,.20f,1});
        for(int i=0;i<5;++i) {
            const auto a=glm::vec2(-12+i*5,12-i*8);
            line(a,a+glm::vec2(-7,-6),1.4f,{.87f,.77f,.56f,1});
            line(a,a+glm::vec2(7,1),1.1f,{.63f,.49f,.31f,1});
        }
        line({-26,32},{18,-29},1.7f,{.83f,.72f,.51f,1});
    }
    void cairn() {
        shadow();
        cube(-16,-12,0,26,23,7,{.39f,.41f,.33f});
        cube(-12,-10,7,19,18,10,{.54f,.54f,.44f});
        cube(-9,-6,17,13,11,11,{.44f,.45f,.36f});
        cube(-5,-4,28,10,9,10,{.57f,.57f,.46f});
        cube(-14,8,4,9,3,3,{.33f,.46f,.14f});
        cube(-12,6,16,5,3,3,{.34f,.45f,.15f});
        leaf({14,15},{24,2},4,{.42f,.51f,.16f});
        leaf({-17,18},{-25,9},3,{.31f,.43f,.12f});
    }
    void scarf() {
        shadow();
        polygon({{-15,-20},{-4,-29},{17,-29},{27,-21},{24,-7},{12,1},{-17,-2},{-24,-10}}, {.49f,.13f,.065f,1});
        polygon({{-15,-20},{-5,-25},{16,-25},{21,-20},{16,-14},{-6,-13}}, {.20f,.065f,.03f,1});
        polygon({{-22,-12},{-9,-6},{11,-8},{26,-18},{24,-7},{9,1},{-17,-2}}, {.68f,.23f,.12f,1});
        polygon({{-15,-2},{-4,0},{-12,33},{-23,30},{-20,17}}, {.57f,.17f,.09f,1});
        polygon({{-4,0},{4,0},{1,21},{-7,25},{-10,20}}, {.39f,.105f,.05f,1});
        for(int i=0;i<4;++i)line({-22+i*3,28},{-24+i*3,34},1.5f,{.54f,.17f,.08f,1});
        line({-18,-7},{2,-3},2,{.79f,.32f,.16f,1});
    }
    void fern() {
        shadow();
        for(int branch=0;branch<6;++branch) {
            const float angle=-2.9f+branch*.52f;
            const glm::vec2 end(std::cos(angle)*31,std::sin(angle)*32-2);
            line({0,19},end,1.7f,{.28f,.36f,.11f,1});
            for(int i=1;i<5;++i) {
                const auto point=glm::mix(glm::vec2(0,19),end,i/5.f);
                const glm::vec2 side(-std::sin(angle),std::cos(angle));
                const auto axis=glm::normalize(end-glm::vec2(0,19));
                leaf(point,point+side*(12-i)+axis*5,2.7f,{.35f+.025f*i,.46f+.02f*i,.14f});
                leaf(point,point-side*(12-i)+axis*5,2.7f,{.26f+.02f*i,.39f+.02f*i,.10f});
            }
        }
    }
    void pickaxe() {
        shadow();line({-21,26},{15,-20},7,{.23f,.14f,.065f,1});
        line({-20,24},{14,-19},3.6f,{.58f,.37f,.16f,1});
        polygon({{-25,-16},{-12,-29},{9,-29},{29,-14},{33,1},{17,-12},{4,-18},{-12,-18}}, {.43f,.54f,.53f,1});
        polygon({{-25,-16},{-12,-29},{9,-29},{4,-22},{-10,-23}}, {.71f,.75f,.65f,1});
        polygon({{9,-29},{29,-14},{33,1},{17,-12},{4,-18},{4,-22}}, {.26f,.39f,.39f,1});
        line({1,-22},{8,-14},5,{.70f,.47f,.20f,1});
    }
    void lantern() {
        shadow();
        line({-7,-24},{-7,-34},2.5f,{.49f,.32f,.12f,1});line({-7,-34},{7,-34},2.5f,{.49f,.32f,.12f,1});
        line({7,-34},{7,-24},2.5f,{.49f,.32f,.12f,1});
        polygon({{-17,-15},{0,-25},{18,-15},{13,-10},{-13,-10}}, {.32f,.39f,.22f,1});
        polygon({{-14,-10},{14,-10},{14,20},{-14,20}}, {.83f,.48f,.12f,1});
        polygon({{-10,-7},{10,-7},{10,16},{-10,16}}, {.98f,.78f,.34f,1});
        polygon({{-5,-5},{4,-5},{5,14},{-5,14}}, {1,.9f,.57f,1});
        for(float x:{-14.f,0.f,14.f})line({x,-11},{x,20},2.7f,{.30f,.22f,.10f,1});
        line({-15,5},{15,5},2,{.39f,.27f,.10f,1});line({-17,21},{17,21},5,{.50f,.34f,.13f,1});
    }
    void compass() {
        shadow();
        for(int layer=0;layer<3;++layer)for(int side=0;side<8;++side){
            const float a=side*.785398f,b=(side+1)*.785398f,r=28-layer*3;
            polygon({{0,0},{std::cos(a)*r,std::sin(a)*r},{std::cos(b)*r,std::sin(b)*r}},
                    layer==0?glm::vec4(.38f,.26f,.12f,1):layer==1?glm::vec4(.75f,.57f,.28f,1):glm::vec4(.80f,.74f,.55f,1));
        }
        for(int side=0;side<8;++side){const float a=side*.785398f;line({std::cos(a)*17,std::sin(a)*17},{std::cos(a)*21,std::sin(a)*21},1,{.40f,.34f,.21f,1});}
        polygon({{-5,3},{9,-21},{4,3}}, {.23f,.51f,.40f,1});polygon({{-5,3},{4,3},{-10,21}}, {.53f,.28f,.15f,1});
        cube(-2,-2,0,4,4,3,{.71f,.53f,.22f});
    }
    void ore() {
        shadow();cube(-16,-12,0,24,22,15,{.31f,.34f,.30f});
        for(int i=0;i<5;++i){const float x=-20+i*8,y=-i%3*5;polygon({{x,y},{x-4,y-17},{x+2,y-26},{x+7,y-15},{x+5,y+4}}, {.17f+.035f*i,.50f,.44f,1});polygon({{x+2,y-26},{x+7,y-15},{x+5,y+4},{x+2,y-7}}, {.35f,.72f,.58f,1});}
    }
    void sword() {
        shadow();polygon({{-5,9},{-5,-26},{0,-38},{6,-26},{6,9}}, {.43f,.57f,.55f,1});
        polygon({{-5,9},{-5,-26},{0,-38},{0,9}}, {.73f,.78f,.67f,1});
        line({-15,10},{15,10},5,{.65f,.46f,.21f,1});line({0,14},{0,29},7,{.28f,.15f,.075f,1});
        for(int i=0;i<4;++i)line({-3,16+i*3},{3,18+i*3},1.2f,{.56f,.33f,.13f,1});line({-4,30},{4,30},5,{.65f,.46f,.21f,1});
    }
    void shield() {
        shadow();polygon({{-27,-23},{0,-31},{27,-23},{23,11},{13,25},{0,34},{-13,25},{-23,11}}, {.43f,.33f,.18f,1});
        polygon({{-23,-20},{0,-27},{23,-20},{19,10},{10,22},{0,29},{-10,22},{-19,10}}, {.67f,.66f,.48f,1});
        polygon({{-19,-17},{0,-23},{19,-17},{15,8},{8,19},{0,25},{-8,19},{-15,8}}, {.27f,.39f,.23f,1});
        polygon({{0,-23},{19,-17},{15,8},{8,19},{0,25}}, {.18f,.29f,.18f,1});
        line({0,-17},{0,15},2,{.69f,.69f,.44f,1});leaf({0,7},{-12,-4},4,{.49f,.58f,.28f});leaf({0,-1},{11,-13},4,{.58f,.64f,.33f});
    }
    void leaves() {
        shadow();line({-21,26},{16,-24},2.5f,{.37f,.30f,.13f,1});
        leaf({-11,12},{-30,-6},7,{.33f,.48f,.19f});leaf({-5,4},{19,5},7,{.41f,.54f,.20f});
        leaf({3,-8},{-9,-30},7,{.46f,.58f,.23f});leaf({8,-15},{30,-30},7,{.36f,.50f,.18f});
    }
    void wave() {
        shadow();
        for(int row=0;row<3;++row){const float y=-19+row*17;
            polygon({{-32,y+10},{-20,y},{-5,y+5},{8,y-4},{24,y},{32,y+8},{19,y+4},{8,y+1},{-5,y+11},{-20,y+6}},
                    { .22f+row*.05f,.48f+row*.03f,.46f+row*.02f,1});}
    }
    void root() {
        shadow();line({-2,-30},{-4,-8},9,{.35f,.23f,.11f,1});line({-4,-8},{-23,24},7,{.40f,.27f,.12f,1});
        line({-3,-8},{7,12},8,{.44f,.29f,.13f,1});line({7,12},{28,24},5,{.40f,.25f,.11f,1});
        line({-3,-14},{18,-27},4,{.46f,.31f,.14f,1});line({-10,7},{-29,11},4,{.43f,.29f,.13f,1});
        line({7,12},{2,30},4,{.43f,.29f,.13f,1});leaf({-3,-14},{-22,-27},5,{.42f,.52f,.16f});leaf({11,-22},{28,-19},5,{.34f,.46f,.13f});
    }
    void eye() {
        shadow();polygon({{-32,0},{-16,-16},{0,-21},{17,-15},{32,0},{17,15},{0,20},{-17,15}}, {.48f,.39f,.21f,1});
        polygon({{-25,0},{-13,-10},{0,-14},{14,-10},{25,0},{14,10},{0,14},{-14,10}}, {.86f,.80f,.59f,1});
        polygon({{-9,-10},{1,-14},{11,-9},{14,2},{8,11},{-4,13},{-13,4}}, {.33f,.49f,.30f,1});
        polygon({{-4,-8},{3,-9},{7,-3},{6,5},{-1,9},{-6,3}}, {.15f,.24f,.13f,1});
        polygon({{-4,-7},{0,-9},{3,-5},{0,-2}}, {.92f,.88f,.67f,1});
    }
private:
    glm::vec2 p(glm::vec2 position) const { return center+position*scale; }
    Drawing& out;glm::vec2 center;float scale;
};

template<class Drawing>
void icon(Drawing& out,SkillIcon kind,glm::vec2 center,float size)
{
    Illustration<Drawing> art(out,center,size);
    switch(kind) {
    case SkillIcon::Boots:art.boots();break;
    case SkillIcon::Chestplate:art.chestplate();break;
    case SkillIcon::Feather:art.feather();break;
    case SkillIcon::Cairn:art.cairn();break;
    case SkillIcon::Scarf:art.scarf();break;
    case SkillIcon::Fern:art.fern();break;
    case SkillIcon::Pickaxe:art.pickaxe();break;
    case SkillIcon::Lantern:art.lantern();break;
    case SkillIcon::Compass:art.compass();break;
    case SkillIcon::Ore:art.ore();break;
    case SkillIcon::Sword:art.sword();break;
    case SkillIcon::Shield:art.shield();break;
    case SkillIcon::Leaf:art.leaves();break;
    case SkillIcon::Wave:art.wave();break;
    case SkillIcon::Root:art.root();break;
    case SkillIcon::Eye:art.eye();break;
    }
}

} // namespace voxel::journal
