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
    const auto* newest=chat_compat::select(0x6AB54E37,0x12C01000);
    require(newest==&chat_compat::release_12652,"26.52 PE identity must select the chat profile");
    require(newest->submit_rva==0x4DE1980&&newest->display_rva==0x155EEA0&&
        newest->field_signature_rva==0x4DE19AD&&newest->send_signature_rva==0x4DE1CF2,
        "26.52 verified RVAs changed");
    require(chat_compat::controller_signature_rva(newest)==0x4DE1D01&&
        chat_compat::uses_modern_display(newest),"26.52 controller/display routing changed");
    require(chat_compat::matches_fragments(newest->submit_signature.data(),newest->field_signature.data(),
        newest->send_signature.data(),*newest),"exact 26.52 compound fingerprint rejected");
    require(chat_compat::select(0x6AB54E36,0x12C01000)==nullptr,
        "nearby 26.52 timestamp must fail closed");
    std::cout << "26.50/26.52 chat profiles, fingerprints, and ABI layouts passed.\n";
}
