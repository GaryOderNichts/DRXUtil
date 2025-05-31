#include "FlashScreen.hpp"
#include "Gfx.hpp"
#include "ProcUI.hpp"
#include "Utils.hpp"
#include "Firmware.hpp"
#include <cstdio>

#include <coreinit/mcp.h>
#include <coreinit/thread.h>
#include <coreinit/filesystem_fsa.h>
#include <nsysccr/cdc.h>
#include <nsysccr/cfg.h>
#include <nn/ccr.h>

namespace {

#include "logo.inc"

bool GetDRCFirmwarePath(std::string& path)
{
    int32_t handle = MCP_Open();
    if (handle < 0) {
        return false;
    }

    alignas(0x40) MCPTitleListType title;
    uint32_t titleCount = 0;
    MCPError error = MCP_TitleListByAppType(handle, MCP_APP_TYPE_DRC_FIRMWARE, &titleCount, &title, sizeof(title));
    MCP_Close(handle);

    if (error != 0 || titleCount != 1) {
        return false;
    }

    path = std::string(&title.path[0]) + "/content/drc_fw.bin";
    return true;
}

bool ReadFirmwareHeader(const std::string& path, FlashScreen::FirmwareHeader& header)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }

    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) {
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

bool WriteFile(const std::string& path, std::span<const std::byte> data)
{
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) {
        return false;
    }

    if (fwrite(data.data(), 1, data.size(), f) != data.size()) {
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

bool CopyFile(const std::string& srcPath, const std::string& dstPath)
{
    FILE* inf = fopen(srcPath.c_str(), "rb");
    if (!inf) {
        return false;
    }

    FILE* outf = fopen(dstPath.c_str(), "wb");
    if (!outf) {
        fclose(inf);
        return false;
    }

    uint8_t buf[4096];
    size_t bytesRead;
    while ((bytesRead = fread(buf, 1, sizeof(buf), inf)) > 0) {
        if (fwrite(buf, 1, bytesRead, outf) != bytesRead) {
            fclose(inf);
            fclose(outf);
            return false;
        }
    }

    if (ferror(inf)) {
        fclose(inf);
        fclose(outf);
        return false;
    }

    fclose(inf);
    fclose(outf);
    return true;
}

bool CaffeineInvalidate()
{
    CCRCDCSoftwareVersion version;
    CCRCDCSoftwareGetVersion(CCR_CDC_DESTINATION_DRC0, &version);

    // Only newer versions have caffeine
    if (version.runningVersion >= 0x180a0000) {
        return CCRSysCaffeineSetCaffeineSlot(0xff) == 0;
    }

    return true;
}

bool WaitForEeprom(uint32_t drcSlot)
{
    uint8_t val;
    OSTime startTime = OSGetSystemTime();
    while (CCRCFGGetCachedEeprom(drcSlot, 0, &val, sizeof(val)) == -1) {
        // 2 second timeout
        if (OSTicksToSeconds(OSGetSystemTime() - startTime) > 2) {
            return false;
        }

        OSSleepTicks(OSMillisecondsToTicks(200));
    }

    return true;
}

bool ReattachDRC(CCRCDCDestination dest, CCRCDCDrcStateEnum targetState, BOOL unknown)
{
    // Get the current DRC state
    CCRCDCDrcState state;
    int32_t res = CCRCDCSysGetDrcState(dest, &state);
    if (res != 0) {
        return false;
    }

    if (state.state == CCR_CDC_DRC_STATE_STANDALONE) {
        state.state = CCR_CDC_DRC_STATE_ACTIVE;
    }

    // Nothing to do if we're already in the target state
    if (state.state == targetState) {
        return true;
    }

    __CCRSysInitReattach(dest - CCR_CDC_DESTINATION_DRC0);

    // Set target state
    state.state = targetState;
    res = CCRCDCSysSetDrcState(dest, &state);
    if (res != 0) {
        return false;
    }

    // Wait for the DRC to reattach
    res = __CCRSysWaitReattach(dest - CCR_CDC_DESTINATION_DRC0, unknown);
    if (res != 0) {
        return false;
    }

    // Wait for EEPROM
    if (!WaitForEeprom(dest - CCR_CDC_DESTINATION_DRC0)) {
        return false;
    }

    // Check if we're in the state we want
    res = CCRCDCSysGetDrcState(dest, &state);
    if (res != 0) {
        return false;
    }

    if (state.state != targetState) {
        return false;
    }

    return true;
}

bool AbortUpdate(CCRCDCDestination dest)
{
    OSTime startTime = OSGetSystemTime();
    while (CCRCDCSoftwareAbort(dest) != 0) {
        // 3 second timeout
        if (OSTicksToSeconds(OSGetSystemTime() - startTime) > 3) {
            return false;
        }

        OSSleepTicks(OSMillisecondsToTicks(200));
    }

    return true;
}

void SoftwareUpdateCallback(IOSError error, void* arg)
{
    FlashScreen* flashScreen = static_cast<FlashScreen*>(arg);

    flashScreen->OnUpdateCompleted(error);
}

}

FlashScreen::FlashScreen()
 : mFileEntries({
    {FILE_ORIGINAL, {0xf187, "Original Firmware"}},
    {FILE_BUILTIN,  {0xf462, "Built-in Firmware Patches"}},
    {FILE_SDCARD,   {0xf7c2, "From SD Card (\"sd:/drc_fw.bin\")"}},
 }),
 mErrorString(),
 mFirmwarePath(),
 mFirmwareHeader(),
 mFlashingProgress(0),
 mUpdateComplete(false),
 mUpdateResult(-1),
 mMessageBox()
{
}

FlashScreen::~FlashScreen()
{
}

void FlashScreen::Draw()
{
    DrawTopBar("FlashScreen");

    switch (mState)
    {
        case STATE_SELECT_FILE: {
            for (FileID id = FILE_ORIGINAL; id <= FILE_SDCARD; id = static_cast<FileID>(id + 1)) {
                int yOff = 75 + static_cast<int>(id) * 150;
                Gfx::DrawRectFilled(0, yOff, Gfx::SCREEN_WIDTH, 150, Gfx::COLOR_ALT_BACKGROUND);
                Gfx::DrawIcon(68, yOff + 150 / 2, 60, Gfx::COLOR_TEXT, mFileEntries[id].icon);
                Gfx::Print(128 + 8, yOff + 150 / 2, 60, Gfx::COLOR_TEXT, mFileEntries[id].name, Gfx::ALIGN_VERTICAL);

                if (id == mFile) {
                    Gfx::DrawRect(0, yOff, Gfx::SCREEN_WIDTH, 150, 8, Gfx::COLOR_HIGHLIGHTED);
                }
            }
            break;
        }
        case STATE_PREPARE: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT, "Preparing...", Gfx::ALIGN_CENTER);
            break;
        }
        case STATE_UPDATE: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT, "Starting update...", Gfx::ALIGN_CENTER);
            break;
        }
        case STATE_FLASHING: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2 - 32, 64, Gfx::COLOR_TEXT, Utils::sprintf("Flashing... %d%%", mFlashingProgress), Gfx::ALIGN_CENTER);
            Gfx::DrawRect(64, Gfx::SCREEN_HEIGHT / 2 + 32, Gfx::SCREEN_WIDTH - 128, 64, 5, Gfx::COLOR_ACCENT);
            Gfx::DrawRectFilled(64, Gfx::SCREEN_HEIGHT / 2 + 32, (Gfx::SCREEN_WIDTH - 128) * (mFlashingProgress / 100.0f), 64, Gfx::COLOR_ACCENT);
            break;
        }
        case STATE_ACTIVATE: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT, "Activating firmware...", Gfx::ALIGN_CENTER);
            break;
        }
        case STATE_DONE: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT,
                Utils::sprintf("Done!\n"
                               "Flashed firmware version:\n0x%08x (%s)", mFirmwareHeader.version, mFileEntries[mFile].name),
                Gfx::ALIGN_CENTER);
            break;
        }
        case STATE_ERROR: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_ERROR, "Error:\n" + mErrorString, Gfx::ALIGN_CENTER);
            break;
        }
    }

    if (mState == STATE_SELECT_FILE) {
        DrawBottomBar("\ue07d Navigate", "\ue044 Exit", "\ue000 Confirm / \ue001 Back");
    } else if (mState == STATE_PREPARE || mState == STATE_UPDATE || mState == STATE_FLASHING || mState == STATE_ACTIVATE) {
        DrawBottomBar(nullptr, "Please wait...", nullptr);
    } else {
        DrawBottomBar(nullptr, nullptr, "\ue001 Back");
    }

    if (mMessageBox) {
        mMessageBox->Draw();
    }
}

bool FlashScreen::Update(VPADStatus& input)
{
    if (mMessageBox) {
        if (!mMessageBox->Update(input)) {
            mMessageBox.reset();
        }

        return true;
    }

    switch (mState)
    {
        case STATE_SELECT_FILE: {
            if (input.trigger & VPAD_BUTTON_B) {
                return false;
            }

            if (input.trigger & VPAD_BUTTON_A) {
                mMessageBox = std::make_unique<MessageBox>(
                    "Are you sure?",
                    Utils::sprintf("About to flash: %s\n"
                                   "Modifying the firmware can render your Gamepad unusable!\n"
                                   "Never install anything that you don't trust.\n"
                                   "Only continue if you know what you're doing.",
                                   mFileEntries[mFile].name),
                    std::vector{
                        MessageBox::Option{0, "\ue001 Back", [this]() {} },
                        MessageBox::Option{0xf00c, "Continue", [this]() {
                            mState = STATE_PREPARE;
                        }},
                    }
                );
                break;
            }

            if (input.trigger & VPAD_BUTTON_DOWN) {
                if (mFile < FILE_SDCARD) {
                    mFile = static_cast<FileID>(mFile + 1);
                }
            } else if (input.trigger & VPAD_BUTTON_UP) {
                if (mFile > FILE_ORIGINAL) {
                    mFile = static_cast<FileID>(mFile - 1);
                }
            }
            break;
        }
        case STATE_PREPARE: {
            std::string originalFirmwarePath;
            if (!GetDRCFirmwarePath(originalFirmwarePath)) {
                mErrorString = "Failed to get original DRC firmware path";
                mState = STATE_ERROR;
                break;
            }

            // Replace absolute path with devoptab prefix
            std::size_t prefixPos = originalFirmwarePath.find("/vol/storage_mlc01");
            if (prefixPos != 0) {
                mErrorString = "Invalid firmware path";
                mState = STATE_ERROR;
                break;
            }
            std::string doptPath = originalFirmwarePath;
            doptPath.replace(prefixPos, sizeof("/vol/storage_mlc01") - 1, "storage_mlc01:");

            FirmwareHeader originalFirmwareHeader;
            if (!ReadFirmwareHeader(doptPath, originalFirmwareHeader)) {
                mErrorString = "Failed to read original DRC firmware header";
                mState = STATE_ERROR;
                break;
            }

            if (mFile == FILE_ORIGINAL) {
                mFirmwarePath = originalFirmwarePath;
                mFirmwareHeader = originalFirmwareHeader;
            } else if (mFile == FILE_BUILTIN) {
                if (!PatchFirmware()) {
                    mState = STATE_ERROR;
                    break;
                }
            } else if (mFile == FILE_SDCARD) {
                if (!ReadFirmwareHeader("/vol/external01/drc_fw.bin", mFirmwareHeader)) {
                    mErrorString = "Failed to read DRC firmware header";
                    mState = STATE_ERROR;
                    break;
                }

                // Don't allow downgrading lower than the version on NAND,
                // otherwise this might cause bricks without flashing the language files?
                if (mFirmwareHeader.version < originalFirmwareHeader.version) {
                    mErrorString = Utils::sprintf("Not allowing versions lower than version on NAND.\n(Firmware 0x%08x Original 0x%08x)", mFirmwareHeader.version, originalFirmwareHeader.version);
                    mState = STATE_ERROR;
                    break;
                }

                // Copy to MLC so IOS-PAD can install it
                mFirmwarePath = "/vol/storage_mlc01/usr/tmp/drc_fw.bin";
                if (!CopyFile("/vol/external01/drc_fw.bin", "storage_mlc01:/usr/tmp/drc_fw.bin")) {
                    mErrorString = "Failed to copy firmware to MLC";
                    mState = STATE_ERROR;
                    break;
                }
            } else {
                mState = STATE_ERROR;
                break;
            }

            mMessageBox = std::make_unique<MessageBox>(
                "Are you really really really sure?",
                Utils::sprintf("About to flash firmware version 0x%08x.\n"
                               "Flashing a firmware can do permanent damage!!!\n",
                               mFirmwareHeader.version),
                std::vector{
                    MessageBox::Option{0, "\ue001 Back", [this]() {} },
                    MessageBox::Option{0xf00c, "Continue", [this]() {
                        mState = STATE_UPDATE;
                    }},
                }
            );
            // Go back to select file, if the message box closes without selecting continue
            mState = STATE_SELECT_FILE;
            break;
        }
        case STATE_UPDATE: {
            ProcUI::SetHomeButtonMenuEnabled(false);

            if (!CaffeineInvalidate()) {
                mErrorString = "Failed to invalidate caffeine.";
                mState = STATE_ERROR;
                break;  
            }

            // Abort any potential pending software updates
            CCRCDCSoftwareAbort(CCR_CDC_DESTINATION_DRC0);

            // Reattach the DRC in update mode
            if (!ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_FWUPDATE, FALSE)) {
                ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_ACTIVE, FALSE);
                mErrorString = "Failed to reattach DRC in update mode.";
                mState = STATE_ERROR;
                break;  
            }

            mFlashingProgress = 0;
            mUpdateComplete = false;
            mUpdateResult = 0;
            if (CCRCDCSoftwareUpdate(CCR_CDC_DESTINATION_DRC0, mFirmwarePath.c_str(), SoftwareUpdateCallback, this) != 0) {
                AbortUpdate(CCR_CDC_DESTINATION_DRC0);
                ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_ACTIVE, FALSE);
                mErrorString = "Failed to start software update.";
                mState = STATE_ERROR;
                break;  
            }

            mState = STATE_FLASHING;
            break;
        }
        case STATE_FLASHING: {
            // Update progress
            CCRCDCFWInfo fwInfo{};
            if (CCRCDCGetFWInfo(CCR_CDC_DESTINATION_DRC0, &fwInfo) == 0) {
                mFlashingProgress = fwInfo.updateProgress;
            }

            OSSleepTicks(OSMillisecondsToTicks(200));

            // Check if update complete
            if (mUpdateComplete) {
                if (mUpdateResult == IOS_ERROR_OK) {
                    mState = STATE_ACTIVATE;
                } else {
                    AbortUpdate(CCR_CDC_DESTINATION_DRC0);
                    ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_ACTIVE, FALSE);
                    mErrorString = "Software update failed.";
                    mState = STATE_ERROR;
                }
            }
            break;
        }
        case STATE_ACTIVATE: {
            // Activate the newly flashed firmware
            if (CCRCDCSoftwareActivate(CCR_CDC_DESTINATION_DRC0) != 0) {
                AbortUpdate(CCR_CDC_DESTINATION_DRC0);
                ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_ACTIVE, FALSE);
                mErrorString = "Failed to activate software update.";
                mState = STATE_ERROR;
                break;  
            }

            // Put the gamepad back into active mode
            OSTime startTime = OSGetSystemTime();
            while (!ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_ACTIVE, FALSE)) {
                // 10 second timeout
                if (OSTicksToSeconds(OSGetSystemTime() - startTime) > 10) {
                    // At this point we don't really care if it times out or not
                    break;
                }

                OSSleepTicks(OSMillisecondsToTicks(1000));
            }

            mState = STATE_DONE;
            break;
        }
        case STATE_ERROR:
        case STATE_DONE: {
            if (input.trigger & VPAD_BUTTON_B) {
                ProcUI::SetHomeButtonMenuEnabled(true);
                return false;
            }
            break;
        }
        
        default:
            break;
    }

    return true;
}

void FlashScreen::OnUpdateCompleted(int32_t result)
{
    mUpdateComplete = true;
    mUpdateResult = result;
}

bool FlashScreen::PatchFirmware()
{
    std::string firmwarePath;
    if (!GetDRCFirmwarePath(firmwarePath)) {
        mErrorString = "Failed to get firmware path for patching";
        return false;
    }

    FileStream stream(firmwarePath);
    if (stream.GetError() != Stream::ERROR_OK) {
        mErrorString = "Failed to open firmware for patching";
        return false;
    }

    auto blob = FirmwareBlob::FromStream(stream);
    if (!blob) {
        mErrorString = "Failed to parse firmware\n" + blob.error();
        return false;
    }

    if (blob->GetImageVersion() != 0x190c0117) {
        mErrorString = "Invalid image version to patch";
        return false;
    }

    // Patch the LVC section
    auto lvc = blob->GetFirmware().GetSection<GenericSection>("LVC_");
    if (lvc) {
        if (Utils::crc32(lvc->ToBytes()) != 0x82d87264) {
            mErrorString = "Invalid LVC checksum";
            return false;
        }

        // Patch jumptable to make ID 1 valid
        lvc->WriteAt<std::uint8_t> (0x0003c9c4, 0x30);
        // Patch config table at offset 1 to insert region
        lvc->WriteAt<std::uint32_t>(0x000b28d0, 0x3);   // size
        lvc->WriteAt<std::uint32_t>(0x000b28d4, 0x103); // eeprom offset

        // Patch jumptable to make ID 4 valid
        lvc->WriteAt<std::uint8_t> (0x0003c9c7, 0x30);
        // Patch config table at offset 4 to insert board config
        lvc->WriteAt<std::uint32_t>(0x000b28e8, 0x3);   // size
        lvc->WriteAt<std::uint32_t>(0x000b28ec, 0x106); // eeprom offset
    } else {
        mErrorString = "LVC section not found in firmware";
        return false;
    }

    // Add "Modified Firmware" logo to startup screen in resource section
    auto img = blob->GetFirmware().GetSection<ResourceSection>("IMG_");
    if (img) {
        auto startupScreen = img->GetResource<BitmapResource>(0x2001);
        if (startupScreen) {
            startupScreen->BlendBitmapBits(logo, 103u, 400u, 48u);
        } else {
            mErrorString = "Startup screen not found";
            return false;
        }
    } else {
        mErrorString = "IMG section not found";
        return false;
    }

    // Patch version in version section
    auto ver = blob->GetFirmware().GetSection<GenericSection>("VER_");
    if (ver) {
        ver->WriteAt<std::uint32_t>(0, 0xfe000000);
    } else {
        mErrorString = "Version section not found";
        return false;
    }

    blob->SetImageVersion(0xfe000000);

    auto patchedBytes = blob->ToBytes();

// This check can be disabled when experimenting with patches
#if 1
    std::uint32_t crc = Utils::crc32(patchedBytes);
    if (crc != 0xd13694f3 && // JPN
        crc != 0xb0aed5b6 && // USA
        crc != 0x2d177dfb)   // EUR
    {
        mErrorString = "Patched file CRC doesn't match";
        return false;
    }
#endif

    mFirmwarePath = "/vol/storage_mlc01/usr/tmp/drc_fw.bin";
    if (!WriteFile("storage_mlc01:/usr/tmp/drc_fw.bin", patchedBytes)) {
        mErrorString = "Failed to write temporary file to MLC";
        return false;
    }

    mFirmwareHeader.version            = blob->GetImageVersion();
    mFirmwareHeader.blockSize          = blob->GetBlockSize();
    mFirmwareHeader.sequencePerSession = blob->GetSequencePerSession();
    mFirmwareHeader.imageSize          = patchedBytes.size() - sizeof(mFirmwareHeader);

    return true;
}
