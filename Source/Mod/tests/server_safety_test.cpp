#include "../src/integration/ServerSafety.hpp"
#include "../src/framework/Module.hpp"

#include <cstdlib>
#include <cstdio>

using namespace utility::integration::server_safety;

void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}

class Risky final : public utility::Module {
public: std::string_view name() const noexcept override{return "risky";}
};
class Passive final : public utility::Module {
public:
    std::string_view name() const noexcept override{return "passive";}
    bool allowed_on_remote_server() const noexcept override{return true;}
};

int main(){
    reset();
    check(!remote_session(1000)&&!local_world(1000),"No world observation is neutral");
    observe_client_tick(1000);
    check(remote_session(1000)&&!local_world(1000),"Client-only tick fails closed as remote");
    observe_integrated_server_tick(1010);
    check(!remote_session(1010)&&local_world(1010),"Integrated server replica enables local-only modules");
    observe_client_tick(2600);
    check(remote_session(2600)&&!local_world(2600),"Stale integrated-server observation fails closed");
    reset();
    observe_client_tick(GetTickCount64());
    Risky risky;Passive passive;
    check(!risky.usable()&&passive.usable(),"Only explicitly safe modules remain usable remotely");
    reset();
    std::puts("Server safety detection passed.");
}
