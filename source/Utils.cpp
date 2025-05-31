#include "Utils.hpp"
#include <cstring>

namespace Utils
{

constexpr std::uint32_t CRCPOLY = 0xedb88320u;

std::string ToHexString(const void* data, size_t size)
{
    std::string str;
    for (size_t i = 0; i < size; ++i)
        str += Utils::sprintf("%02x", ((const uint8_t*) data)[i]);
    
    return str;
}

std::uint32_t crc32(std::span<const std::byte> bytes)
{
    std::uint32_t crc = 0xFFFFFFFFu;
    
    for(std::size_t i = 0; i < bytes.size(); i++) {
        std::uint8_t ch = std::uint8_t(bytes[i]);
        for(std::size_t j = 0; j < 8; j++) {
            std::uint32_t b = (ch ^ crc) & 1;
            crc >>= 1;
            if (b) {
                crc = crc ^ 0xEDB88320;
            }
            ch >>= 1;
        }
    }
    
    return ~crc;
}

}
