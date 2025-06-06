#pragma once

#include <string>
#include <memory>
#include <span>

namespace Utils
{

template<typename ...Args>
std::string sprintf(const std::string& format, Args ...args)
{
    int size = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1;

    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args ...);

    return std::string(buf.get(), buf.get() + size - 1);
}

std::string ToHexString(const void* data, size_t size);

std::uint32_t crc32(std::span<const std::byte> bytes);

}
