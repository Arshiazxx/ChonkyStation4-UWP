#pragma once

#include <Common.hpp>
#include <TinySHA1.hpp>
#include <array>


namespace Helpers {

// From shadPS4
// The NID is the SHA1 hash of the function name + salt.
static std::string nameToNid(const std::string& name) {
    const std::array<u8, 16> salt = { 0x51, 0x8D, 0x64, 0xA6, 0x35, 0xDE, 0xD8, 0xC1, 0xE6, 0xB0, 0x39, 0xB1, 0xC3, 0xE5, 0x52, 0x30 };
    
    std::vector<u8> input;
    input.resize(name.size() + salt.size());
    std::memcpy(input.data(), name.data(), name.size());
    std::memcpy(input.data() + name.size(), salt.data(), salt.size());

    u64 digest;
    sha1::SHA1 sha;
    sha.processBytes(input.data(), input.size());
    sha.getDigestBytes((u8*)&digest);

    const std::string codes = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";
    std::string nid(11, '\0');
    for (int i = 0; i < 10; i++) {
        nid[i] = codes[(digest >> (58 - i * 6)) & 0x3f];
    }
    nid[10] = codes[(digest & 0xf) * 4];

    return nid;
}

}