#include "../src/integration/ChatCompatibility.hpp"
#include "../src/integration/NativeChat.hpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace utility::integration;

namespace {
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
}

int main() {
    const auto* current=chat_compat::select(0x6AA482FD,0x12C01000);
    require(current==&chat_compat::release_12650,"26.50 PE identity must select the chat profile");
    require(chat_compat::select(0x6AA482FD,0x12C01001)==nullptr,"nearby image size must fail closed");
    require(chat_compat::select(0x6AA482FC,0x12C01000)==nullptr,"nearby timestamp must fail closed");
    require(current->submit_rva==0x4DE18B0 && current->display_rva==0x155F190,
        "26.50 verified RVAs changed");
    require(chat_compat::controller_12650_signature_rva==0x4DE1C31 &&
        chat_compat::controller_12650_signature[3]==0x48 &&
        chat_compat::controller_12650_signature[4]==0x0D &&
        chat_compat::controller_12650_signature[10]==0x48,
        "26.50 controller model/validity layout fingerprint changed");
    auto submit=current->submit_signature;
    auto field=current->field_signature;
    auto send=current->send_signature;
    require(chat_compat::matches_fragments(submit.data(),field.data(),send.data(),*current),
        "exact 26.50 compound fingerprint rejected");
    submit[3]^=1;
    require(!chat_compat::matches_fragments(submit.data(),field.data(),send.data(),*current),
        "changed submit prologue accepted");
    submit=current->submit_signature; field[6]^=1;
    require(!chat_compat::matches_fragments(submit.data(),field.data(),send.data(),*current),
        "changed input layout accepted");
    field=current->field_signature; send[12]^=1;
    require(!chat_compat::matches_fragments(submit.data(),field.data(),send.data(),*current),
        "changed native-send branch accepted");
    require(sizeof(native_chat::NativeString)==32 && sizeof(native_chat::OptionalString)==40 &&
        sizeof(native_chat::GuiDataHandle)==24,"release chat ABI layout changed");
    std::cout << "26.50 chat profile, fingerprints, and ABI layouts passed.\n";
}
