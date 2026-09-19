#include "../src/integration/BedrockBuild.hpp"

#include <cstdio>
#include <cstdlib>

namespace {
void check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
}

int main() {
    using namespace utility::integration;
    check(classify_bedrock_build(0x6A8378BA, 0x12888000) == BedrockBuildKind::release_12645,
          "legacy 1.26.45 profile");
    check(classify_bedrock_build(0x6AA482FD, 0x12C01000) == BedrockBuildKind::release_12650,
          "installed 1.26.50 profile");
    check(classify_bedrock_build(0x6AA482FD, 0x12888000) == BedrockBuildKind::unsupported,
          "mixed metadata must fail closed");
    check(classify_bedrock_build(0, 0) == BedrockBuildKind::unsupported,
          "unknown metadata must fail closed");
    std::puts("PASS: Bedrock 1.26.45 and installed 1.26.50 profiles classify exactly");
}
