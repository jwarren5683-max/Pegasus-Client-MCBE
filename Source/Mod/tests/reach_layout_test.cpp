#include "../src/modules/ReachModule.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <cstddef>

using namespace utility::modules;
void check(bool ok,const char* text) {
    if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}
}
int main() {
    // Constructor/methods are compiled separately, just like the real factory.
    check(reach_implementation_size()==sizeof(ReachModule),"factory and implementation agree on allocation size");
    alignas(ReachModule) std::array<std::byte,sizeof(ReachModule)+64> memory;
    memory.fill(std::byte{0xA5});
    auto* reach=new(memory.data()) ReachModule;
    check(!reach->available(),"unattached reach has no native hooks");
    reach->set_value(10);reach->set_block_value(10);
    reach->set_block_enabled(true);reach->on_enable();
    check(reach->value()==10&&reach->block_value()==10,"cross-file sliders retain values");
    reach->set_block_enabled(false);reach->on_disable();
    reach->~ReachModule();
    for(size_t i=sizeof(ReachModule);i<memory.size();++i)
        check(memory[i]==std::byte{0xA5},"constructor and methods never write beyond factory allocation");
    std::printf("PASS: separate-file reach allocation %zu bytes; canary intact\n",sizeof(ReachModule));
}

