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
        for(const Request expected:std::array{Request{25,false},Request{-3,true},Request{0,true}}){
            const auto actual=decode(encode(expected));
            require(actual.amount==expected.amount&&actual.levels==expected.levels,"XP request encoding changed");
        }
        auto bytes=command_dispatch_signature;
        require(dispatch_signature_matches(bytes.data()),"verified 26.52 XP dispatch signature rejected");
        bytes[7]^=std::byte{1};
        require(!dispatch_signature_matches(bytes.data()),"changed XP dispatch signature accepted");
        pending.store(encode({7,true}));
        require(apply_pending(nullptr)==ApplyResult::rejected&&!pending.load(),"unsupported host did not fail closed");
        require(request(1,false)==RequestResult::unavailable,"non-Bedrock test host accepted XP request");
        std::cout<<"XP request encoding, 26.52 dispatch evidence and fail-closed gates passed.\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
