#include "DrcFlashScreen.hpp"
#include "Gfx.hpp"
#include "ProcUI.hpp"
#include "Utils.hpp"
#include <cstdio>

#include <coreinit/mcp.h>
#include <coreinit/thread.h>
#include <coreinit/filesystem_fsa.h>
#include <nsysccr/cdc.h>
#include <nsysccr/cfg.h>
#include <nn/ccr.h>

namespace {

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

void SoftwareUpdateCallback(IOSError error, void* arg)
{
    DrcFlashScreen* drcFlashScreen = static_cast<DrcFlashScreen*>(arg);

    drcFlashScreen->OnUpdateCompleted(error);
}

}

DrcFlashScreen::DrcFlashScreen()
 : mFileEntries({
    {FILE_ORIGINAL, {0xf187, "Original Firmware"}},
    {FILE_SDCARD,   {0xf7c2, "From SD Card (\"sd:/drc_fw.bin\")"}},
 })
{
}

DrcFlashScreen::~DrcFlashScreen()
{
}

void DrcFlashScreen::Draw()
{
    DrawTopBar("DrcFlashScreen");

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
        case STATE_CONFIRM: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT,
                Utils::sprintf("Are you sure?\n"
                               "About to flash: %s", mFileEntries[mFile].name),
                Gfx::ALIGN_CENTER);
            break;
        }
        case STATE_PREPARE: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT, "Preparing...", Gfx::ALIGN_CENTER);
            break;
        }
        case STATE_CONFIRM2: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_ERROR,
                Utils::sprintf("Are you really really really sure?\n"
                               "About to flash firmware version 0x%08x.\n"
                               "Flashing a firmware can do permanent damage!!!\n", mFirmwareHeader.version),
                Gfx::ALIGN_CENTER);
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
    } else if (mState == STATE_CONFIRM || mState == STATE_CONFIRM2) {
        DrawBottomBar(nullptr, "\ue044 Exit", "\ue000 Confirm / \ue001 Back");
    } else if (mState == STATE_PREPARE || mState == STATE_UPDATE || mState == STATE_FLASHING || mState == STATE_ACTIVATE) {
        DrawBottomBar(nullptr, "Please wait...", nullptr);
    } else {
        DrawBottomBar(nullptr, nullptr, "\ue001 Back");
    }
}

bool DrcFlashScreen::Update(VPADStatus& input)
{
    switch (mState)
    {
        case STATE_SELECT_FILE: {
            if (input.trigger & VPAD_BUTTON_B) {
                return false;
            }

            if (input.trigger & VPAD_BUTTON_A) {
                mState = STATE_CONFIRM;
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
        case STATE_CONFIRM:
        case STATE_CONFIRM2: {
            if (input.trigger & VPAD_BUTTON_B) {
                return false;
            }

            if (input.trigger & VPAD_BUTTON_A) {
                mState = (mState == STATE_CONFIRM) ? STATE_PREPARE : STATE_UPDATE;
                break;
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

            FlashUtils::FirmwareHeader originalFirmwareHeader;
            if (!FlashUtils::ReadFirmwareHeader(doptPath, originalFirmwareHeader)) {
                mErrorString = "Failed to read original DRC firmware header";
                mState = STATE_ERROR;
                break;
            }

            if (mFile == FILE_ORIGINAL) {
                mFirmwarePath = originalFirmwarePath;
                mFirmwareHeader = originalFirmwareHeader;
            } else if (mFile == FILE_SDCARD) {
                if (!FlashUtils::ReadFirmwareHeader("/vol/external01/drc_fw.bin", mFirmwareHeader)) {
                    mErrorString = "Failed to read DRC firmware header";
                    mState = STATE_ERROR;
                    break;
                }
/*
                // Don't allow downgrading lower than the version on NAND,
                // otherwise this might cause bricks without flashing the language files?
                if (mFirmwareHeader.version < originalFirmwareHeader.version) {
                    mErrorString = Utils::sprintf("Not allowing versions lower than version on NAND.\n(Firmware 0x%08x Original 0x%08x)", mFirmwareHeader.version, originalFirmwareHeader.version);
                    mState = STATE_ERROR;
                    break;
                }*/

                // Copy to MLC so IOS-PAD can install it
                mFirmwarePath = "/vol/storage_mlc01/usr/tmp/drc_fw.bin";
                if (!FlashUtils::CopyFile("/vol/external01/drc_fw.bin", "storage_mlc01:/usr/tmp/drc_fw.bin")) {
                    mErrorString = "Failed to copy firmware to MLC";
                    mState = STATE_ERROR;
                    break;
                }
            } else {
                mState = STATE_ERROR;
                break;
            }
            
            uint32_t extId = 0;
            if (CCRCDCSoftwareGetExtId(CCR_CDC_DESTINATION_DRC0, CCR_CDC_EXT_LANGUAGE, &extId) != 0) {
                mErrorString = "Failed to get DRC language version!";
                mState = STATE_ERROR;
                break;  
            }
            if (!FlashUtils::CheckVersionSafety(mFirmwareHeader.version, extId)) {
                mErrorString = "Firmware version not valid for the DRC language!";
                mState = STATE_ERROR;
                break;
            }

            mState = STATE_CONFIRM2;
            break;
        }
        case STATE_UPDATE: {
            ProcUI::SetHomeButtonMenuEnabled(false);

            if (!FlashUtils::CaffeineInvalidate()) {
                mErrorString = "Failed to invalidate caffeine.";
                mState = STATE_ERROR;
                break;  
            }

            // Abort any potential pending software updates
            CCRCDCSoftwareAbort(CCR_CDC_DESTINATION_DRC0);

            // Reattach the DRC in update mode
            if (!FlashUtils::ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_FWUPDATE, FALSE)) {
                FlashUtils::ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_ACTIVE, FALSE);
                mErrorString = "Failed to reattach DRC in update mode.";
                mState = STATE_ERROR;
                break;  
            }

            mFlashingProgress = 0;
            mUpdateComplete = false;
            mUpdateResult = 0;
            if (CCRCDCSoftwareUpdate(CCR_CDC_DESTINATION_DRC0, mFirmwarePath.c_str(), SoftwareUpdateCallback, this) != 0) {
                FlashUtils::AbortUpdate(CCR_CDC_DESTINATION_DRC0);
                FlashUtils::ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_ACTIVE, FALSE);
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
                    FlashUtils::AbortUpdate(CCR_CDC_DESTINATION_DRC0);
                    FlashUtils::ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_ACTIVE, FALSE);
                    mErrorString = "Software update failed.";
                    mState = STATE_ERROR;
                }
            }
            break;
        }
        case STATE_ACTIVATE: {
            // Activate the newly flashed firmware
            if (CCRCDCSoftwareActivate(CCR_CDC_DESTINATION_DRC0) != 0) {
                FlashUtils::AbortUpdate(CCR_CDC_DESTINATION_DRC0);
                FlashUtils::ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_ACTIVE, FALSE);
                mErrorString = "Failed to activate software update.";
                mState = STATE_ERROR;
                break;  
            }

            // Put the gamepad back into active mode
            OSTime startTime = OSGetSystemTime();
            while (!FlashUtils::ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_ACTIVE, FALSE)) {
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
        case STATE_DONE: {
            if (input.trigger & VPAD_BUTTON_B) {
                ProcUI::SetHomeButtonMenuEnabled(true);
                return false;
            }
            break;
        }
        case STATE_ERROR: {
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

void DrcFlashScreen::OnUpdateCompleted(int32_t result)
{
    mUpdateComplete = true;
    mUpdateResult = result;
}
