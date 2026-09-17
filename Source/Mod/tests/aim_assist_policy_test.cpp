#include "../src/modules/AimAssistPolicy.hpp"

#include <cassert>
#include <iostream>

using namespace utility::modules::aim_assist;

static Input active() { return Input{true,true,true,true,true}; }

int main() {
    auto input=active();
    Target target{true,true,100.0F,-50.0F,4.0F};
    auto correction=correct(input,target);
    assert(correction.adjust&&correction.dx==10&&correction.dy==-10); // bounded smoothing
    target.error_x=181.0F;target.error_y=0;
    assert(!correct(input,target).adjust); // outside the FOV
    target=Target{true,false,10.0F,0.0F,2.0F};
    assert(!correct(input,target).adjust); // players only
    target=Target{true,true,1.0F,1.0F,2.0F};
    input.modifier=false;assert(!correct(input,target).adjust);
    input=active();input.local_world=false;assert(!correct(input,target).adjust);
    input=active();target.depth=0;assert(!correct(input,target).adjust);
    input=active();target=Target{true,true,1.0F,1.0F,2.0F};
    correction=correct(input,target);assert(!correction.adjust&&correction.dx==0&&correction.dy==0);
    std::cout<<"Aim Assist policy tests passed (local, explicit-hold, bounded smoothing).\n";
}

