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
        auto bytes=add_experience_signature;
        require(signature_matches(bytes.data(),add_experience_signature),"verified 26.52 addExperience signature rejected");
        bytes[7]^=std::byte{1};
        require(!signature_matches(bytes.data(),add_experience_signature),"changed addExperience signature accepted");
        require(signature_matches(add_levels_signature.data(),add_levels_signature),"verified 26.52 addLevels signature rejected");
        require(apply(nullptr,7,true)==ApplyResult::unavailable,"non-Bedrock test host accepted XP request");
        std::cout<<"Horion-style XP 26.52 Player method evidence and compatibility validation passed.\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
