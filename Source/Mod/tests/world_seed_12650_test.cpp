#include "../src/integration/WorldSeed12650.hpp"

#include <cstdlib>
#include <iostream>

using namespace utility::integration::world_seed_12650;

namespace {
void require(bool condition,const char* message) {
    if(!condition){std::cerr<<message<<'\n';std::exit(1);}
}
}

int main() {
    require(block_source_get_level_slot==0x160&&block_source_get_level_rva==0x3462F0,
        "26.50 BlockSource Level accessor changed");
    require(level_get_seed_rva==0x11788A0&&level_data_vtable_slot==0x560,
        "26.50 Level seed ABI profile changed");
    require(block_source_get_level_signature==std::array<Byte,5>{0x48,0x8B,0x41,0x20,0xC3},
        "26.50 BlockSource Level fingerprint changed");
    require(level_get_seed_signature[13]==0x80&&level_get_seed_signature[14]==0x60&&
        level_get_seed_signature[15]==0x05,"26.50 LevelData virtual slot fingerprint changed");
    LevelSeed64Abi output{0xFEDCBA9876543210ULL},other{};
    require(returned_expected_buffer(&output,&output),"valid native sret buffer rejected");
    require(!returned_expected_buffer(nullptr,&output)&&!returned_expected_buffer(&other,&output),
        "foreign native seed result buffer accepted");
    require(output.value==0xFEDCBA9876543210ULL,"signed 64-bit seed bits changed");
    std::cout<<"26.50 world-seed targets, fingerprints, and sret ABI passed.\n";
}
