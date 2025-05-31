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
#include "Firmware.hpp"
#include "Utils.hpp"
#include <algorithm>
#include <cstring>
#include <utility>
#include <ranges>
#include <cassert>

Resource::Resource(Type type, std::uint16_t id, std::vector<std::byte>&& data)
 : mType(type), mId(id), mData(std::move(data))
{
}

Resource::~Resource()
{
}

BitmapResource::BitmapResource(std::uint16_t id, std::uint32_t format, std::uint32_t width, std::uint32_t height, std::vector<std::byte>&& data)
 : Resource(Resource::Type::BITMAP, id, std::move(data)), mFormat(format), mWidth(width), mHeight(height)
{
}

BitmapResource::~BitmapResource()
{
}

std::span<const std::uint32_t, 256> BitmapResource::GetPalette() const
{
    return std::span<const std::uint32_t, 256>(reinterpret_cast<const std::uint32_t*>(mData.data()), 256);
}

std::uint8_t BitmapResource::GetPixel(std::uint32_t x, std::uint32_t y) const
{
    if (x > mWidth || y > mHeight) {
        return false;
    }

    return std::uint8_t(mData.at((256 * 4) + y * mWidth + x));
}

void BitmapResource::BlendBitmap(std::span<const std::uint8_t> pixels, std::uint32_t width, std::uint32_t height)
{
    if (width > mWidth || height > mHeight) {
        return;
    }

    if (width * height > pixels.size()) {
        return;
    }

    for (std::uint32_t y = 0; y < height; y++) {
        for (std::uint32_t x = 0; x < width; x++) {
            std::uint8_t pixel = pixels[y * width + x];
            
            // Most palettes have the last elements all black/transparent, so skip over those to do very basic "blending"
            if (pixel == 0xFF) {
                continue;
            }

            mData[(256 * 4) + y * mWidth + x] = std::byte(pixel);
        }
    }
}

void BitmapResource::BlendBitmapBits(std::span<const std::uint8_t> bits, std::uint8_t paletteIdx, std::uint32_t width, std::uint32_t height)
{
    if (width > mWidth || height > mHeight) {
        return;
    }

    if (width * height > bits.size() * 8) {
        return;
    }

    for (std::uint32_t y = 0; y < height; y++) {
        for (std::uint32_t x = 0; x < width; x++) {
            if ((bits[y * (width / 8) + (x / 8)] >> (x % 8)) & 1) {
                mData[(256 * 4) + y * mWidth + x] = std::byte(paletteIdx);
            }
        }
    }
}

SoundResource::SoundResource(std::uint16_t id, std::uint16_t format, std::uint16_t bits, std::uint32_t channels, std::uint32_t frequency, std::vector<std::byte>&& data)
 : Resource(Resource::Type::SOUND, id, std::move(data)), mFormat(format), mBits(bits), mChannels(channels), mFrequency(frequency)
{
}

SoundResource::~SoundResource()
{
}

UnknownResource::UnknownResource(Type type, std::uint16_t id, std::vector<std::byte>&& parameters, std::vector<std::byte>&& data)
 : Resource(type, id, std::move(data)), mParameters(std::move(parameters))
{
}

UnknownResource::~UnknownResource()
{
}

FirmwareSection::FirmwareSection(const std::array<char, 4>& name, std::uint32_t version)
 : mName(name), mVersion(version)
{
}

FirmwareSection::~FirmwareSection()
{
}

std::optional<FirmwareSection::Header> FirmwareSection::Header::FromStream(Stream& stream)
{
    FirmwareSection::Header header;

    stream.SetEndianness(std::endian::little);
    stream >> header.offset;
    stream >> header.size;
    stream >> header.name;
    stream >> header.version;

    if (stream.GetError() != Stream::ERROR_OK) {
        return std::nullopt;
    }

    return header;
}

ResourceSection::ResourceSection(const std::array<char, 4>& name, std::uint32_t version)
 : FirmwareSection(name, version)
{

}

ResourceSection::~ResourceSection()
{
}

std::shared_ptr<ResourceSection> ResourceSection::FromBytes(const std::array<char, 4>& name, std::uint32_t version, std::span<const std::byte> bytes)
{
    std::shared_ptr<ResourceSection> resourceSection(new ResourceSection(name, version));
    SpanStream stream(bytes, std::endian::little);

    std::uint32_t descriptorCount;
    stream >> descriptorCount; 

    std::size_t dataPosition = stream.GetPosition() + descriptorCount * Resource::DESCRIPTOR_SIZE;

    // Unpack resources
    for (std::uint32_t i = 0; i < descriptorCount; i++) {
        Resource::Type type;
        std::uint16_t id;
        std::uint32_t offset;
        std::uint32_t size;
        stream >> type;
        stream >> id;
        stream >> offset;
        stream >> size;

        if (stream.GetError() != Stream::ERROR_OK) {
            return nullptr;
        }

        // Read resource data
        std::vector<std::byte> data(size);
        std::size_t previousPosition = stream.GetPosition();
        stream.SetPosition(dataPosition + offset);
        stream.Read(data);
        stream.SetPosition(previousPosition);

        if (stream.GetError() != Stream::ERROR_OK) {
            return nullptr;
        }

        // Unpack resource specific parameters
        if (type == Resource::Type::BITMAP) {
            std::uint32_t format;
            std::uint32_t width;
            std::uint32_t height;
            stream >> format;
            stream >> width;
            stream >> height;
            resourceSection->mResources.emplace_back(std::make_shared<BitmapResource>(id, format, width, height, std::move(data)));
        } else if (type == Resource::Type::SOUND) {
            std::uint16_t format;
            std::uint16_t bits;
            std::uint32_t channels;
            std::uint32_t frequency;
            stream >> format;
            stream >> bits;
            stream >> channels;
            stream >> frequency;
            resourceSection->mResources.emplace_back(std::make_shared<SoundResource>(id, format, bits, channels, frequency, std::move(data)));
        } else {
            std::vector<std::byte> param(12);
            stream >> std::span{param};
            resourceSection->mResources.emplace_back(std::make_shared<UnknownResource>(type, id, std::move(param), std::move(data)));
        }

        if (stream.GetError() != Stream::ERROR_OK) {
            return nullptr;
        }
    }

    return resourceSection;
}

std::vector<std::byte> ResourceSection::ToBytes() const
{
    std::vector<std::byte> bytes;
    VectorStream stream(bytes, std::endian::little);

    stream << std::uint32_t(mResources.size());

    std::size_t descriptorPosition = stream.GetPosition();

    // reserve space for resource descriptors
    const std::size_t descriptorsSize = mResources.size() * Resource::DESCRIPTOR_SIZE;
    for (std::size_t i = 0; i < descriptorsSize; i++) {
        stream << std::byte(0);
    }

    // Write resource data
    std::size_t dataPosition = stream.GetPosition();
    std::vector<std::size_t> resourceOffsets;
    for (auto resource : mResources) {
        resourceOffsets.push_back(stream.GetPosition() - dataPosition);
        stream.Write(resource->GetData());
    }

    // Write resource descriptors
    stream.SetPosition(descriptorPosition);
    for (auto [resource, offset] : std::views::zip(mResources, resourceOffsets)) {
        stream << resource->GetType();
        stream << resource->GetID();
        stream << std::uint32_t(offset);
        stream << std::uint32_t(resource->GetData().size());

        if (resource->GetType() == Resource::Type::BITMAP) {
            auto bitmap = std::dynamic_pointer_cast<BitmapResource>(resource);
            assert(bitmap != nullptr);

            stream << bitmap->GetFormat();
            stream << bitmap->GetWidth();
            stream << bitmap->GetHeight();
        } else if (resource->GetType() == Resource::Type::SOUND) {
            auto sound = std::dynamic_pointer_cast<SoundResource>(resource);
            assert(sound != nullptr);

            stream << sound->GetFormat();
            stream << sound->GetBits();
            stream << sound->GetChannels();
            stream << sound->GetFrequency();
        } else {
            auto unknown = std::dynamic_pointer_cast<UnknownResource>(resource);
            assert(unknown != nullptr);

            stream << std::span{unknown->GetParameters()};
        }
    }

    return bytes;
}

std::shared_ptr<Resource> ResourceSection::GetResource(std::uint16_t id)
{
    for (auto resource : mResources) {
        if (resource->GetID() == id) {
            return resource;
        }
    }

    return nullptr;
}

GenericSection::GenericSection(const std::array<char, 4>& name, std::uint32_t version, std::vector<std::byte>&& data)
 : FirmwareSection(name, version), mData(data)
{

}

GenericSection::~GenericSection()
{
}

void GenericSection::WriteAt(std::size_t offset, std::span<const std::byte> data)
{
    if (offset + data.size() > mData.size()) {
        return;
    }

    std::copy_n(data.begin(), data.size(), mData.begin() + offset);
}

Firmware::Firmware()
{
}

Firmware::~Firmware()
{
}

std::expected<Firmware, std::string> Firmware::FromStream(Stream& stream)
{
    Firmware fw;

    // The firmware itself is little endian
    stream.SetEndianness(std::endian::little);

    if (!fw.UnpackHeader(stream)) {
        return std::unexpected("Firmware header verification failed");
    }

    if (stream.GetError() != Stream::ERROR_OK) {
        return std::unexpected("Stream read failed");
    }

    if (!fw.UnpackSections(stream)) {
        return std::unexpected("Failed to unpack sections");
    }

    return fw;
}

std::vector<std::byte> Firmware::ToBytes() const
{
    std::vector<std::byte> bytes;
    VectorStream stream(bytes, std::endian::little);

    auto sectionData = PackSections();

    // Calculate subCRCs
    std::vector<std::byte> subCRCData(0x4000, std::byte(0));
    VectorStream subCRCStream(subCRCData, std::endian::little);
    for (std::size_t i = 0; i < sectionData.size(); i += 0x1000) {
        std::size_t size = 0x1000;
        if (sectionData.size() - i < size) {
            size = sectionData.size() - i;
        }

        // Write crc
        subCRCStream << Utils::crc32(std::span{sectionData.data() + i, size});
    }

    // Write header
    stream << mType;
    // super CRCs
    for (std::size_t i = 0; i < 4; i++) {
        stream << Utils::crc32(std::span{subCRCData.data() + i * 0x1000, 0x1000});
    }
    // padding
    for (std::size_t i = 0; i < 0xFE8; i++) {
        stream << std::byte(0);
    }
    // header CRC
    stream << Utils::crc32(std::span{bytes.data(), 0xFFC});

    // append sub crcs and section data
    std::copy(subCRCData.begin(), subCRCData.end(), std::back_inserter(bytes));
    std::copy(sectionData.begin(), sectionData.end(), std::back_inserter(bytes));

    return bytes;
}

std::shared_ptr<FirmwareSection> Firmware::GetSection(std::string name)
{
    if (name.size() != 4u) {
        return nullptr;
    }

    for (auto section : mSections) {
        if (std::memcmp(section->GetName().data(), name.data(), 4u) == 0) {
            return section;
        }
    }

    return nullptr;
}

bool Firmware::UnpackHeader(Stream& stream)
{
    std::vector<std::byte> header(0x1000);
    std::vector<std::byte> subCRCData(0x4000);
    stream.Read(std::span{header});
    stream.Read(std::span{subCRCData});

    if (stream.GetError() != Stream::ERROR_OK) {
        return false;
    }

    // Unpack little endian firmware header
    SpanStream headerStream(std::span{header}, std::endian::little);
    std::array<std::uint32_t, 4> superCRCs;
    std::uint32_t headerCRC;
    headerStream >> mType;
    headerStream >> superCRCs;
    headerStream.Skip(0xFE8); // Padding
    headerStream >> headerCRC;

    // Verify header CRC
    if (headerCRC != Utils::crc32(std::span{header.data(), header.size() - sizeof(std::uint32_t)})) {
        return false;
    }

    // Verify super CRCs
    for (std::size_t i = 0; i < superCRCs.size(); ++i) {
        if (superCRCs[i] != Utils::crc32(std::span{subCRCData.data() + i * 0x1000, 0x1000})) {
            return false;
        }
    }

    size_t fwStartPosition = stream.GetPosition();

    // Verify subCrcs
    SpanStream subCrcStream(std::span{subCRCData}, std::endian::little);
    std::vector<std::byte> buffer(0x1000);
    for (std::size_t i = 0; i < stream.GetRemaining(); i += 0x1000) {
        std::size_t size = 0x1000;
        if (size > stream.GetRemaining()) {
            size = stream.GetRemaining();
        }

        stream.Read(std::span{buffer.begin(), size});
        if (stream.GetError() != Stream::ERROR_OK) {
            return false;
        }

        std::uint32_t crc;
        subCrcStream >> crc;
        if (crc != Utils::crc32(std::span{buffer.data(), size})) {
            return false;
        }
    }

    stream.SetPosition(fwStartPosition);
    return true;
}

bool Firmware::UnpackSections(Stream& stream)
{
    size_t fwStartPosition = stream.GetPosition();

    // Unpack first firmware section header (should be INDX section)
    auto indexHeader = FirmwareSection::Header::FromStream(stream);
    if (!indexHeader) {
        return false;
    }

    // Make sure this was actually the INDX section
    if (std::memcmp(indexHeader->name.data(), "INDX", 4u) != 0) {
        return false;
    }

    // Make sure the index was supposed to be here
    if (indexHeader->offset != 0u) {
        return false;
    }

    // Unpack all sections headers
    stream.SetPosition(fwStartPosition);
    std::size_t numSections = indexHeader->size / FirmwareSection::Header::SIZE;
    for (std::size_t i = 0; i < numSections; i++) {
        // Read section header
        auto header = FirmwareSection::Header::FromStream(stream);
        if (!header) {
            return false;
        }

        // Read section data
        std::vector<std::byte> data(header->size);
        std::size_t previousPosition = stream.GetPosition();
        stream.SetPosition(fwStartPosition + header->offset);
        stream.Read(data);
        stream.SetPosition(previousPosition);

        if (stream.GetError() != Stream::ERROR_OK) {
            return false;
        }

        if (std::memcmp(header->name.data(), "IMG_", 4u) == 0) {
            auto section = ResourceSection::FromBytes(header->name, header->version, std::move(data));
            if (!section) {
                return false;
            }

            mSections.emplace_back(section);
        } else {
            mSections.emplace_back(std::make_shared<GenericSection>(header->name, header->version, std::move(data)));
        }
    }

    return true;
}

std::vector<std::byte> Firmware::PackSections() const
{
    std::vector<std::byte> bytes;
    VectorStream stream(bytes, std::endian::little);

    // reserve space for indx section
    const std::size_t indxSize = mSections.size() * FirmwareSection::Header::SIZE;
    for (std::size_t i = 0; i < indxSize; i++) {
        stream << std::byte(0);
    }

    // pack sections (skip over indx sections)
    std::vector<FirmwareSection::Header> sectionHeaders(mSections.size());
    for (std::size_t i = 1; i < mSections.size(); i++) {
        auto sectionData = mSections[i]->ToBytes();

        // Prepare header
        FirmwareSection::Header& section = sectionHeaders.at(i);
        section.name = mSections[i]->GetName();
        section.size = sectionData.size();
        section.offset = stream.GetPosition();
        section.version = mSections[i]->GetVersion();

        // Write data
        stream.Write(sectionData);
    }

    // prepare indx section header
    FirmwareSection::Header& indxSection = sectionHeaders.at(0);
    indxSection.name =  mSections[0]->GetName();
    indxSection.size = indxSize;
    indxSection.offset = 0;
    indxSection.version = mSections[0]->GetVersion();

    // pack section headers
    stream.SetPosition(0);
    for (const auto& section : sectionHeaders) {
        stream << section.offset;
        stream << section.size;
        stream << section.name;
        stream << section.version;
    }

    return bytes;
}

FirmwareBlob::FirmwareBlob()
{
}

FirmwareBlob::~FirmwareBlob()
{
}

std::expected<FirmwareBlob, std::string> FirmwareBlob::FromStream(Stream& stream)
{
    FirmwareBlob blob;
    std::uint32_t imageSize;

    // Unpack big endian firmware blob header
    stream.SetEndianness(std::endian::big);
    stream >> blob.mImageVersion;
    stream >> blob.mBlockSize;
    stream >> blob.mSequencePerSession;
    stream >> imageSize;

    if (stream.GetError() != Stream::ERROR_OK) {
        return std::unexpected("Stream read failed");
    }

    // Make sure enough data remains in the stream to unpack the firmware
    if (stream.GetRemaining() != imageSize) {
        return std::unexpected("File size doesn't match image size");
    }

    // Unpack firmware
    auto fw = Firmware::FromStream(stream);
    if (!fw) {
        return std::unexpected(fw.error());
    }
    blob.mFirmware = *fw;

    return blob;
}

std::vector<std::byte> FirmwareBlob::ToBytes() const
{
    std::vector<std::byte> bytes;
    VectorStream stream(bytes, std::endian::big);

    auto firmwareData = mFirmware.ToBytes();

    // Pack header
    stream << mImageVersion;
    stream << mBlockSize;
    stream << mSequencePerSession;
    stream << std::uint32_t(firmwareData.size());

    std::copy(firmwareData.begin(), firmwareData.end(), std::back_inserter(bytes));

    return bytes;
}
