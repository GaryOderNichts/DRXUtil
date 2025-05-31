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
#pragma once

#include <bit>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include <utility>

template<typename T>
concept Enum = std::is_enum_v<std::remove_cvref_t<T>>;

class Stream {
public:
    enum Error {
        ERROR_OK,
        ERROR_OPEN_FAILED,
        ERROR_READ_FAILED,
        ERROR_WRITE_FAILED,
    };

public:
    Stream(std::endian endianness = std::endian::native);
    virtual ~Stream();

    Error GetError() const;

    void SetEndianness(std::endian endianness);
    std::endian GetEndianness() const;

    virtual std::size_t Read(const std::span<std::byte>& data) = 0;
    virtual std::size_t Write(const std::span<const std::byte>& data) = 0;

    virtual bool SetPosition(std::size_t position) = 0;
    virtual std::size_t GetPosition() const = 0;
    bool Skip(int offset);

    virtual std::size_t GetRemaining() const = 0;

    // Stream read operators
    template<std::integral T>
    Stream& operator>>(T& val)
    {
        val = 0;
        if (Read(std::as_writable_bytes(std::span(std::addressof(val), 1))) == sizeof(val)) {
            if (NeedsSwap()) {
                val = std::byteswap(val);
            }
        }

        return *this;
    }
    Stream& operator>>(std::byte& val);
    Stream& operator>>(bool& val);
    Stream& operator>>(float& val);
    Stream& operator>>(double& val);
    template<typename T, std::size_t N>
    Stream& operator>>(std::span<T, N> val)
    {
        for (size_t i = 0; i < val.size(); i++) {
            *this >> val[i];
        }

        return *this;
    }
    template<typename T, std::size_t N>
    Stream& operator>>(std::array<T, N>& val)
    {
        *this >> std::span{val};
        return *this;
    }
    template<Enum T>
    Stream& operator>>(T& val)
    {
        std::underlying_type_t<T> tmp;
        *this >> tmp;
        val = static_cast<T>(tmp);
        return *this;
    }

    // Stream write operators
    template<std::integral T>
    Stream& operator<<(T val)
    {
        if (NeedsSwap()) {
            val = std::byteswap(val);
        }

        Write(std::as_bytes(std::span(std::addressof(val), 1)));
        return *this;
    }
    Stream& operator<<(std::byte val);
    Stream& operator<<(bool val);
    Stream& operator<<(float val);
    Stream& operator<<(double val);
    template<typename T, std::size_t N>
    Stream& operator<<(std::span<const T, N> val)
    {
        for (size_t i = 0; i < val.size(); i++) {
            *this << val[i];
        }

        return *this;
    }
    template<typename T, std::size_t N>
    Stream& operator<<(const std::array<T, N>& val)
    {
        *this << std::span{val};
        return *this;
    }
    template<Enum T>
    Stream& operator<<(T val)
    {
        *this << std::to_underlying(val);
        return *this;
    }

protected:
    void SetError(Error error);

    bool NeedsSwap();

    Error mError;
    std::endian mEndianness;
};

class FileStream : public Stream {
public:
    FileStream(const std::string& path, std::endian endianness = std::endian::native);
    virtual ~FileStream();

    virtual std::size_t Read(const std::span<std::byte>& data) override;
    virtual std::size_t Write(const std::span<const std::byte>& data) override;

    virtual bool SetPosition(std::size_t position) override;
    virtual std::size_t GetPosition() const override;

    virtual std::size_t GetRemaining() const override;

private:
    std::unique_ptr<std::FILE, int(*)(std::FILE*)> mFile;
    std::size_t mSize;
};

class VectorStream : public Stream {
public:
    VectorStream(std::vector<std::byte>& vector, std::endian endianness = std::endian::native);
    virtual ~VectorStream();

    virtual std::size_t Read(const std::span<std::byte>& data) override;
    virtual std::size_t Write(const std::span<const std::byte>& data) override;

    virtual bool SetPosition(std::size_t position) override;
    virtual std::size_t GetPosition() const override;

    virtual std::size_t GetRemaining() const override;

private:
    std::reference_wrapper<std::vector<std::byte>> mVector;
    std::size_t mPosition;
};

class SpanStream : public Stream {
public:
    SpanStream(std::span<const std::byte> span, std::endian endianness = std::endian::native);
    virtual ~SpanStream();

    virtual std::size_t Read(const std::span<std::byte>& data) override;
    virtual std::size_t Write(const std::span<const std::byte>& data) override;

    virtual bool SetPosition(std::size_t position) override;
    virtual std::size_t GetPosition() const override;

    virtual std::size_t GetRemaining() const override;

private:
    std::span<const std::byte> mSpan;
    std::size_t mPosition;
};
