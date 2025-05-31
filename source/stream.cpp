/*
 *   Copyright (C) 2024 GaryOderNichts
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "stream.hpp"

#include <algorithm>

Stream::Stream(std::endian endianness)
 : mError(ERROR_OK), mEndianness(endianness)
{
}

Stream::~Stream()
{
}

Stream::Error Stream::GetError() const
{
    return mError;
}

void Stream::SetEndianness(std::endian endianness)
{
    mEndianness = endianness;
}

std::endian Stream::GetEndianness() const
{
    return mEndianness;
}

bool Stream::Skip(int offset)
{
    int position = GetPosition() + offset;
    if (position < 0) {
        return false;
    }

    return SetPosition(position);
}

Stream& Stream::operator>>(std::byte& val)
{
    std::uint8_t i;
    *this >> i;
    val = std::byte(i);

    return *this;
}

Stream& Stream::operator>>(bool& val)
{
    std::uint8_t i;
    *this >> i;
    val = !!i;

    return *this;
}

Stream& Stream::operator>>(float& val)
{
    std::uint32_t i;
    *this >> i;
    val = std::bit_cast<float>(i);

    return *this;
}

Stream& Stream::operator>>(double& val)
{
    std::uint64_t i;
    *this >> i;
    val = std::bit_cast<double>(i);

    return *this;
}

Stream& Stream::operator<<(std::byte val)
{
    std::uint8_t i = static_cast<std::uint8_t>(val);
    *this << i;

    return *this;
}

Stream& Stream::operator<<(bool val)
{
    std::uint8_t i = val;
    *this << i;

    return *this;
}

Stream& Stream::operator<<(float val)
{
    std::uint32_t i = std::bit_cast<std::uint32_t>(val);
    *this << i;

    return *this;
}

Stream& Stream::operator<<(double val)
{
    std::uint64_t i = std::bit_cast<std::uint64_t>(val);
    *this << i;

    return *this;
}

void Stream::SetError(Error error)
{
    mError = error;
}

bool Stream::NeedsSwap()
{
    return mEndianness != std::endian::native;
}

FileStream::FileStream(const std::string& path, std::endian endianness)
: Stream(endianness), mFile(nullptr, nullptr)
{
    mFile = std::unique_ptr<std::FILE, int(*)(std::FILE*)>(std::fopen(path.c_str(), "rb"), &std::fclose);
    if (!mFile) {
        SetError(ERROR_OPEN_FAILED);
        return;
    }

    std::fseek(mFile.get(), 0, SEEK_END);
    mSize = std::ftell(mFile.get());
    std::fseek(mFile.get(), 0, SEEK_SET);
}

FileStream::~FileStream()
{
}

std::size_t FileStream::Read(const std::span<std::byte>& data)
{
    if (!mFile) {
        SetError(ERROR_OPEN_FAILED);
        return 0;
    }

    size_t read = std::fread(data.data(), 1u, data.size(), mFile.get());
    if (read != data.size()) {
        SetError(ERROR_READ_FAILED);
    }

    return read;
}

std::size_t FileStream::Write(const std::span<const std::byte>& data)
{
    // if (!mFile) {
    //     SetError(ERROR_OPEN_FAILED);
    //     return 0;
    // }

    // size_t written = std::fwrite(data.data(), 1u, data.size(), mFile.get());
    // if (written != data.size()) {
    //     SetError(ERROR_WRITE_FAILED);
    // }

    // return written;

    // TODO read only for now
    SetError(ERROR_WRITE_FAILED);
    return 0;
}

bool FileStream::SetPosition(std::size_t position)
{
    if (!mFile) {
        SetError(ERROR_OPEN_FAILED);
        return false;
    }

    return std::fseek(mFile.get(), position, SEEK_SET) == 0;
}

std::size_t FileStream::GetPosition() const
{
    if (!mFile) {
        return 0;
    }

    return std::ftell(mFile.get());
}

std::size_t FileStream::GetRemaining() const
{
    return mSize - GetPosition();
}

VectorStream::VectorStream(std::vector<std::byte>& vector, std::endian endianness)
 : Stream(endianness), mVector(vector), mPosition(0)
{
}

VectorStream::~VectorStream()
{
}

std::size_t VectorStream::Read(const std::span<std::byte>& data)
{
    if (data.size() > GetRemaining()) {
        SetError(ERROR_READ_FAILED);
        return 0;
    }

    std::copy_n(mVector.get().begin() + mPosition, data.size(), data.begin());
    mPosition += data.size();
    return data.size();
}

std::size_t VectorStream::Write(const std::span<const std::byte>& data)
{
    // Resize vector if not enough bytes remain
    if (mPosition + data.size() > mVector.get().size()) {
        mVector.get().resize(mPosition + data.size());
    }

    std::copy(data.begin(), data.end(), mVector.get().begin() + mPosition);
    mPosition += data.size();
    return data.size();
}

bool VectorStream::SetPosition(std::size_t position)
{
    if (position >= mVector.get().size()) {
        return false;
    }

    mPosition = position;
    return true;
}

std::size_t VectorStream::GetPosition() const
{
    return mPosition;
}

std::size_t VectorStream::GetRemaining() const
{
    return mVector.get().size() - mPosition;
}

SpanStream::SpanStream(std::span<const std::byte> span, std::endian endianness)
 : Stream(endianness), mSpan(std::move(span)), mPosition(0)
{
}

SpanStream::~SpanStream()
{
}

std::size_t SpanStream::Read(const std::span<std::byte>& data)
{
    if (data.size() > GetRemaining()) {
        SetError(ERROR_READ_FAILED);
        return 0;
    }

    std::copy_n(mSpan.begin() + mPosition, data.size(), data.begin());
    mPosition += data.size();
    return data.size();
}

std::size_t SpanStream::Write(const std::span<const std::byte>& data)
{
    // Cannot write to const span
    SetError(ERROR_WRITE_FAILED);
    return 0;
}

bool SpanStream::SetPosition(std::size_t position)
{
    if (position >= mSpan.size()) {
        return false;
    }

    mPosition = position;
    return true;
}

std::size_t SpanStream::GetPosition() const
{
    return mPosition;
}

std::size_t SpanStream::GetRemaining() const
{
    if (mPosition > mSpan.size()) {
        return 0;
    }

    return mSpan.size() - mPosition;
}
