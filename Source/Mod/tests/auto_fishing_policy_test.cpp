#include "../src/modules/AutoFishingPolicy.hpp"

#include <cstdlib>
#include <iostream>

using namespace utility::modules::auto_fishing;

namespace {
void require(bool condition,const char* message) {
    if(!condition){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}
}
Input safe(bool hook=false,std::uint64_t id=0,float y=0.0F) {
    return {true,true,true,true,true,true,hook,id,y};
}
}

int main() {
    require(!click_release_due(100,144),"right-click remains held through the next frame");
    require(click_release_due(100,145),"right-click releases after the minimum hold");
    require(click_release_due(100,99),"clock rollback releases a held button");
    Controller controller;
    require(controller.update(100,safe()).cast,"enable performs one initial cast");
    require(!controller.update(200,safe()).cast,"missing hook does not spam casts");
    require(!controller.update(300,safe(true,7,10.0F)).reel,"new hook is observed without reeling");
    require(!controller.update(1450,safe(true,7,9.98F)).reel,"small bob is ignored");
    require(controller.update(1500,safe(true,7,9.85F)).reel,"sudden mature-hook drop reels");
    require(!controller.update(1600,safe()).cast,"recast waits for hook disappearance delay");
    require(controller.update(1950,safe()).cast,"recast follows a completed reel");

    controller.reset();
    require(controller.update(10,safe()).cast,"reset rearms initial cast");
    require(!controller.update(4000,safe()).cast,"failed cast times out without retry spam");
    require(controller.update(5000,safe()).cast,"failed cast gets one delayed retry");
    require(!controller.update(8000,safe()).cast,"second cast remains quiet until its timeout");
    require(!controller.update(8501,safe()).cast,"second failed cast stops without a third retry");
    require(!controller.update(9000,safe(true,9,5.0F)).reel,"manual hook appearance rearms observation");

    controller.reset();
    auto input=safe();input.local_world=false;
    require(!controller.update(100,input).cast,"remote sessions never cast");
    input=safe();input.foreground=false;
    require(!controller.update(200,input).cast,"background game never casts");
    input=safe();input.rod_selected=false;
    require(!controller.update(250,input).cast,"non-rod held item never casts");
    input=safe();input.sample_valid=false;
    require(!controller.update(300,input).cast,"invalid actor snapshot fails closed");

    controller.reset();
    require(controller.update(100,safe()).cast,"clock test initial cast");
    require(!controller.update(200,safe(true,4,8.0F)).reel,"clock test observes hook");
    require(!controller.update(1500,safe(true,4,7.7F)).reel,"stale sample gap cannot trigger reel");

    std::cout<<"Auto Fishing policy tests passed.\n";
}
