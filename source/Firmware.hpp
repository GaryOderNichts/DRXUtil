/*
 *   Copyright (C) 2025 GaryOderNichts
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
#include <cstdint>
#include <expected>
#include <optional>
#include <array>

#include "stream.hpp"

class Resource {
public:
    enum class Type : std::uint16_t {
        BITMAP = 0x0,
        SOUND = 0x1,
        UNKNOWN = 0x2,
    };

    static constexpr std::size_t DESCRIPTOR_SIZE = 0x18;

    Resource(Type type, std::uint16_t id, std::vector<std::byte>&& data);
    virtual ~Resource();

    static std::shared_ptr<Resource> FromStream(Stream& stream);

    Type GetType() const { return mType; }
    std::uint16_t GetID() const { return mId; }
    const std::vector<std::byte>& GetData() const { return mData; }

protected:
    Type mType;
    std::uint16_t mId;
    std::vector<std::byte> mData;
};

class BitmapResource : public Resource {
public:
    BitmapResource(std::uint16_t id, std::uint32_t format, std::uint32_t width, std::uint32_t height, std::vector<std::byte>&& data);
    virtual ~BitmapResource();

    std::uint32_t GetFormat() const { return mFormat; }
    std::uint32_t GetWidth() const { return mWidth; }
    std::uint32_t GetHeight() const { return mHeight; }

    std::span<const std::uint32_t, 256> GetPalette() const;
    std::uint8_t GetPixel(std::uint32_t x, std::uint32_t y) const;

    void BlendBitmap(std::span<const std::uint8_t> pixels, std::uint32_t width, std::uint32_t height);
    void BlendBitmapBits(std::span<const std::uint8_t> bits, std::uint8_t paletteIdx, std::uint32_t width, std::uint32_t height);

protected:
    std::uint32_t mFormat;
    std::uint32_t mWidth;
    std::uint32_t mHeight;
};

class SoundResource : public Resource {
public:
    SoundResource(std::uint16_t id, std::uint16_t format, std::uint16_t bits, std::uint32_t channels, std::uint32_t frequency, std::vector<std::byte>&& data);
    virtual ~SoundResource();

    std::uint16_t GetFormat() const { return mFormat; }
    std::uint16_t GetBits() const { return mBits; }
    std::uint32_t GetChannels() const { return mChannels; }
    std::uint32_t GetFrequency() const { return mFrequency; }

protected:
    std::uint16_t mFormat;
    std::uint16_t mBits;
    std::uint32_t mChannels;
    std::uint32_t mFrequency;
};

class UnknownResource : public Resource {
public:
    UnknownResource(Type type, std::uint16_t id, std::vector<std::byte>&& parameters, std::vector<std::byte>&& data);
    virtual ~UnknownResource();

    const std::vector<std::byte>& GetParameters() const { return mParameters; }

protected:
    std::vector<std::byte> mParameters;
};

class FirmwareSection {
public:
    struct Header {
        std::uint32_t offset;
        std::uint32_t size;
        std::array<char, 4> name;
        std::uint32_t version;

        static constexpr std::size_t SIZE = 0x10;

        static std::optional<FirmwareSection::Header> FromStream(Stream& stream);
    };

    FirmwareSection(const std::array<char, 4>& name, std::uint32_t version);
    virtual ~FirmwareSection();

    virtual std::vector<std::byte> ToBytes() const = 0;

    const std::array<char, 4>& GetName() const { return mName; }
    std::uint32_t GetVersion() const { return mVersion; }

protected:
    std::array<char, 4> mName;
    std::uint32_t mVersion;
};

template <typename T>
concept ResourceConcept = std::is_base_of<Resource, T>::value;

class ResourceSection : public FirmwareSection {
private:
    ResourceSection(const std::array<char, 4>& name, std::uint32_t version);

public:
    virtual ~ResourceSection();

    static std::shared_ptr<ResourceSection> FromBytes(const std::array<char, 4>& name, std::uint32_t version, std::span<const std::byte> bytes);
    std::vector<std::byte> ToBytes() const override;

    std::shared_ptr<Resource> GetResource(std::uint16_t id);
    template<ResourceConcept T>
    std::shared_ptr<T> GetResource(std::uint16_t id)
    {
        return std::dynamic_pointer_cast<T>(GetResource(id));
    }

    // Allow iterating over resources in section
    auto begin() { return mResources.begin(); }
    auto end() { return mResources.end(); }
    auto begin() const { return mResources.begin(); }
    auto end() const { return mResources.end(); }

protected:
    std::vector<std::shared_ptr<Resource>> mResources;
};

class GenericSection : public FirmwareSection {
public:
    GenericSection(const std::array<char, 4>& name, std::uint32_t version, std::vector<std::byte>&& data);
    virtual ~GenericSection();

    std::vector<std::byte> ToBytes() const override { return mData; };

    void WriteAt(std::size_t offset, std::span<const std::byte> data);
    template<std::integral T>
    void WriteAt(std::size_t offset, T val)
    {
        if (std::endian::little != std::endian::native) {
            val = std::byteswap(val);
        }

        WriteAt(offset, std::as_bytes(std::span(std::addressof(val), 1)));
    }

protected:
    std::vector<std::byte> mData;
};

template <typename T>
concept SectionConcept = std::is_base_of<FirmwareSection, T>::value;

class Firmware {
public:
    enum class Type : std::uint32_t {
        DRC = 0x01010000,
        DRH = 0x00010000,
    };

    struct Header {
        Type type;
        std::array<std::uint32_t, 4> superCRCs;
        std::array<std::byte, 0xFE8> padding;
        uint32_t headerCRC;
        std::array<std::uint32_t, 0x1000> subCRCs;
    };

    Firmware();
    virtual ~Firmware();

    static std::expected<Firmware, std::string> FromStream(Stream& stream);
    std::vector<std::byte> ToBytes() const;

    std::shared_ptr<FirmwareSection> GetSection(std::string name);
    template<SectionConcept T>
    std::shared_ptr<T> GetSection(std::string name)
    {
        return std::dynamic_pointer_cast<T>(GetSection(name));
    }

    // Allow iterating over sections in firmware
    auto begin() { return mSections.begin(); }
    auto end() { return mSections.end(); }
    auto begin() const { return mSections.begin(); }
    auto end() const { return mSections.end(); }

protected:
    bool UnpackHeader(Stream& stream);
    bool UnpackSections(Stream& stream);

    std::vector<std::byte> PackSections() const;

    Type mType;
    std::vector<std::shared_ptr<FirmwareSection>> mSections;
};

class FirmwareBlob {
public:
    FirmwareBlob();
    virtual ~FirmwareBlob();

    static std::expected<FirmwareBlob, std::string> FromStream(Stream& stream);
    std::vector<std::byte> ToBytes() const;

    std::uint32_t GetImageVersion() const { return mImageVersion; }
    std::uint32_t GetBlockSize() const { return mBlockSize; }
    std::uint32_t GetSequencePerSession() const { return mSequencePerSession; }

    void SetImageVersion(std::uint32_t version) { mImageVersion = version; }

    const Firmware& GetFirmware() const { return mFirmware; }
    Firmware& GetFirmware() { return mFirmware; }

protected:
    std::uint32_t mImageVersion;
    std::uint32_t mBlockSize;
    std::uint32_t mSequencePerSession;
    Firmware mFirmware;
};
