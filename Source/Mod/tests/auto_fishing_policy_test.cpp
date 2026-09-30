#include "../src/modules/AutoFishingPolicy.hpp"

#include <cstdlib>
#include <iostream>

using namespace utility::modules::auto_fishing;

namespace {
void require(bool condition,const char* message) {
    if(!condition){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}
}
Input safe(bool hook=false,std::uint64_t id=0,float y=0.0F) {
    Input input{};
    input.enabled=true;
    input.foreground=true;
    input.alive=true;
    input.rod_selected=true;
    input.sample_valid=true;
    input.hook_present=hook;
    input.hook_id=id;
    input.hook_y=y;
    return input;
}
}

int main() {
    require(!click_release_due(100,209),"right-click remains held across multiple input ticks");
    require(click_release_due(100,210),"right-click releases after the reliable hold");
    require(click_release_due(100,99),"clock rollback releases a held button");
    require(rod_read_grace_active(100,1600),"temporary empty rod reads stay verified during use animation");
    require(!rod_read_grace_active(100,1601),"rod-read grace expires promptly");
    require(!rod_read_grace_active(100,99),"clock rollback cannot extend rod-read grace");
    Controller controller;
    require(controller.update(100,safe()).cast,"enable performs one initial cast");
    require(!controller.update(200,safe()).cast,"missing hook does not spam casts");
    require(!controller.update(300,safe(true,7,10.0F)).reel,"new hook is observed without reeling");
    require(!controller.update(1450,safe(true,7,9.98F)).reel,"small bob is ignored");
    require(controller.update(1500,safe(true,7,9.85F)).reel,"sudden mature-hook drop reels");
    require(!controller.update(1600,safe()).cast,"recast waits for hook disappearance delay");
    require(controller.update(1950,safe()).cast,"recast follows a completed reel");

    controller.reset();
    require(controller.update(100,safe()).cast,"stuck-hook test performs initial cast");
    require(!controller.update(200,safe(true,12,6.0F)).reel,"stuck-hook test observes hook");
    require(!controller.update(1450,safe(true,12,5.98F)).reel,"stuck-hook test ignores normal bobbing");
    require(controller.update(1500,safe(true,12,5.8F)).reel,"stuck-hook test detects bite");
    require(!controller.update(3499,safe(true,12,5.8F)).cast,"lingering hook gets a bounded recovery window");
    require(controller.update(3500,safe(true,12,5.8F)).cast,"lingering hook cannot stall the fishing cycle forever");

    controller.reset();
    require(controller.update(10,safe()).cast,"reset rearms initial cast");
    require(!controller.update(2511,safe()).cast,"failed cast enters a quiet retry delay");
    require(!controller.update(3110,safe()).cast,"failed cast does not retry early");
    require(controller.update(3111,safe()).cast,"failed cast gets a prompt delayed retry");
    require(!controller.update(5612,safe()).cast,"second failed cast enters another bounded delay");
    require(controller.update(6212,safe()).cast,"second missed input gets the final retry");
    require(!controller.update(8713,safe()).cast,"third failed cast stops without input spam");
    require(!controller.update(9000,safe(true,9,5.0F)).reel,"manual hook appearance rearms observation");

    controller.reset();
    auto input=safe();input.foreground=false;
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
