#include "DrcLangScreen.hpp"
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

void SoftwareUpdateCallback(IOSError error, void* arg)
{
    DrcLangScreen* drcLangScreen = static_cast<DrcLangScreen*>(arg);

    drcLangScreen->OnUpdateCompleted(error);
}

}

DrcLangScreen::~DrcLangScreen()
{
}

void DrcLangScreen::Draw()
{
    DrawTopBar("DrcLangScreen");

    switch (mState)
    {
        case STATE_PREPARE: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT, "Preparing...", Gfx::ALIGN_CENTER);
            break;
        }
        case STATE_CONFIRM2: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_ERROR,
                Utils::sprintf("Are you really really really sure?\n"
                               "About to flash langpack.\n"
                               "Lanuage packs mismatching the firmware\nwill brick your GamePad!\n"),
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
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT, "Activating language pack...", Gfx::ALIGN_CENTER);
            break;
        }
        case STATE_DONE: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT,
                Utils::sprintf("Done!\n"
                               "Flashed new language pack"),
                Gfx::ALIGN_CENTER);
            break;
        }
        case STATE_ERROR: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_ERROR, "Error:\n" + mErrorString, Gfx::ALIGN_CENTER);
            break;
        }
    }

    if (mState == STATE_CONFIRM2) {
        DrawBottomBar(nullptr, "\ue044 Exit", "\ue000 Confirm / \ue001 Back");
    } else if (mState == STATE_PREPARE || mState == STATE_UPDATE || mState == STATE_FLASHING || mState == STATE_ACTIVATE) {
        DrawBottomBar(nullptr, "Please wait...", nullptr);
    } else if (mState == STATE_DONE || mState == STATE_ERROR) {
        DrawBottomBar(nullptr, nullptr, "\ue001 Back");
    }
}

bool DrcLangScreen::Update(VPADStatus& input)
{
    FlashUtils::ReadFirmwareHeader("/vol/external01/lang.bin", mFirmwareHeader);
    uint32_t targetVersion = mFirmwareHeader.version;
    switch (mState)
    {
        case STATE_CONFIRM2: {
            if (input.trigger & VPAD_BUTTON_B) {
                return false;
            }

            if (input.trigger & VPAD_BUTTON_A) {
                mState = STATE_UPDATE;
                break;
            }
            break;
        }
        case STATE_PREPARE: {
                // Perform compatibility check
                // Writing only language, must obtain firmware from DRC
                CCRCDCSoftwareVersion deviceVersion;
                CCRCDCSoftwareGetVersion(CCR_CDC_DESTINATION_DRC0, &deviceVersion);
                if (!FlashUtils::CheckVersionSafety(deviceVersion.runningVersion, targetVersion)) {
                    mErrorString = "Language version not valid for the DRC firmware!";
                    mState = STATE_ERROR;
                    break;
                }
                // Copy to MLC so IOS-PAD can install it
                mFirmwarePath = "/vol/storage_mlc01/usr/tmp/lang.bin";
                if (!FlashUtils::CopyFile("/vol/external01/lang.bin", "storage_mlc01:/usr/tmp/lang.bin")) {
                    mErrorString = "Failed to copy firmware to MLC";
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
            if (CCRCDCSoftwareLangUpdate(CCR_CDC_DESTINATION_DRC0, mFirmwarePath.c_str(), &targetVersion, SoftwareUpdateCallback, this) != 0) {
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
            // Activate the newly flashed language
            uint32_t langActivateSuccess = 0;
            CCRCDCSoftwareLangActivate(CCR_CDC_DESTINATION_DRC0, targetVersion, &langActivateSuccess);
            if (langActivateSuccess != 0) {
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

void DrcLangScreen::OnUpdateCompleted(int32_t result)
{
    mUpdateComplete = true;
    mUpdateResult = result;
}
