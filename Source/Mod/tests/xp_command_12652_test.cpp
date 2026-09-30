#include "../src/integration/XpCommand12652.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

using namespace utility::integration::xp_12652;

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
}

int main(){
    try{
        auto bytes=command_dispatch_signature;
        require(dispatch_signature_matches(bytes.data()),"verified 26.52 XP dispatch signature rejected");
        bytes[7]^=std::byte{1};
        require(!dispatch_signature_matches(bytes.data()),"changed XP dispatch signature accepted");
        require(apply(nullptr,7,true)==ApplyResult::unavailable,"non-Bedrock test host accepted XP request");
        std::cout<<"XP 26.52 dispatch evidence and compatibility validation passed.\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
