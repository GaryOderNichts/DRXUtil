#include "Utils.hpp"
#include <cstring>

namespace Utils
{

std::string ToHexString(const void* data, size_t size)
{
    std::string str;
    for (size_t i = 0; i < size; ++i)
        str += Utils::sprintf("%02x", ((const uint8_t*) data)[i]);
    
    return str;
}

}
