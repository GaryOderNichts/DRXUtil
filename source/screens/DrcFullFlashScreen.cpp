#include "DrcFullFlashScreen.hpp"
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
    DrcFullFlashScreen* drcFullFlashScreen = static_cast<DrcFullFlashScreen*>(arg);

    drcFullFlashScreen->OnUpdateCompleted(error);
}

}

DrcFullFlashScreen::~DrcFullFlashScreen()
{
}

void DrcFullFlashScreen::Draw()
{
    DrawTopBar("DrcFullFlashScreen");

    switch (mState)
    {
        case STATE_PREPARE: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT, "Preparing...", Gfx::ALIGN_CENTER);
            break;
        }
        case STATE_CONFIRM2: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_ERROR,
                Utils::sprintf("Are you really really really sure?\n"
                               "About to flash all firmware.\n"
                               "You may brick the DRC!!\n"),
                Gfx::ALIGN_CENTER);
            break;
        }
        case STATE_UPDATE_LANG:
        case STATE_UPDATE_FIRM: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT, "Starting update...", Gfx::ALIGN_CENTER);
            break;
        }
        case STATE_FLASHING_LANG: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2 - 32, 64, Gfx::COLOR_TEXT, Utils::sprintf("Updating language... %d%% done.\nDo not turn the power off.", mFlashingProgress), Gfx::ALIGN_CENTER);
            Gfx::DrawRect(64, Gfx::SCREEN_HEIGHT / 2 + 80, Gfx::SCREEN_WIDTH - 128, 64, 5, Gfx::COLOR_ACCENT);
            Gfx::DrawRectFilled(64, Gfx::SCREEN_HEIGHT / 2 + 80, (Gfx::SCREEN_WIDTH - 128) * (mFlashingProgress / 100.0f), 64, Gfx::COLOR_ACCENT);
            break;
        }
        case STATE_FLASHING_FIRM: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2 - 32, 64, Gfx::COLOR_TEXT, Utils::sprintf("Updating firmware... %d%% done.\nDo not turn the power off.", mFlashingProgress), Gfx::ALIGN_CENTER);
            Gfx::DrawRect(64, Gfx::SCREEN_HEIGHT / 2 + 80, Gfx::SCREEN_WIDTH - 128, 64, 5, Gfx::COLOR_ACCENT);
            Gfx::DrawRectFilled(64, Gfx::SCREEN_HEIGHT / 2 + 80, (Gfx::SCREEN_WIDTH - 128) * (mFlashingProgress / 100.0f), 64, Gfx::COLOR_ACCENT);
            break;
        }
        case STATE_ACTIVATE: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT, "Activating update...", Gfx::ALIGN_CENTER);
            break;
        }
        case STATE_DONE: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT,
                Utils::sprintf("Done!\n"
                               "DRC was updated successfully."),
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
    } else if (mState == STATE_PREPARE || mState == STATE_UPDATE_FIRM || mState == STATE_UPDATE_LANG || mState == STATE_FLASHING_FIRM || mState == STATE_FLASHING_LANG || mState == STATE_ACTIVATE) {
        DrawBottomBar(nullptr, "Please wait...", nullptr);
    } else if (mState == STATE_DONE || mState == STATE_ERROR) {
        DrawBottomBar(nullptr, nullptr, "\ue001 Back");
    }
}

bool DrcFullFlashScreen::Update(VPADStatus& input) // Here is the core logic
{
    flashUtils.ReadFirmwareHeader("/vol/external01/lang.bin", mLanguageHeader);
    uint32_t targetVersion = mLanguageHeader.version; // We will read this value later
    flashUtils.ReadFirmwareHeader("/vol/external01/drc_fw.bin", mFirmwareHeader);
    uint32_t firmwareVersion = mFirmwareHeader.version;
    switch (mState)
    {
        case STATE_CONFIRM2: { // Ask the user before starting writes
            if (input.trigger & VPAD_BUTTON_B) {
                return false;
            }

            if (input.trigger & VPAD_BUTTON_A) {
                mState = STATE_UPDATE_LANG;
                break;
            }
            break;
        }
        case STATE_PREPARE: { // When picked, copy firmware and language to MLC.
		if (!flashUtils.CheckVersionSafety(firmwareVersion, targetVersion)) {
                    mErrorString = "Language version not valid for the DRC firmware!";
                    mState = STATE_ERROR;
                    break;
                }
                // Copy to MLC so IOS-PAD can install it
                mFirmwarePath = "/vol/storage_mlc01/usr/tmp/drc_fw.bin"; // copy firmware
                if (!flashUtils.CopyFile("/vol/external01/drc_fw.bin", "storage_mlc01:/usr/tmp/drc_fw.bin")) {
                    mErrorString = "Failed to copy firmware to MLC";
                    mState = STATE_ERROR;
                    break;
                }
                mLangPath = "/vol/storage_mlc01/usr/tmp/lang.bin"; // copy language
                if (!flashUtils.CopyFile("/vol/external01/lang.bin", "storage_mlc01:/usr/tmp/lang.bin")) {
                    mErrorString = "Failed to copy language to MLC";
                    mState = STATE_ERROR;
                    break;
                }
            mState = STATE_CONFIRM2; // ask user
            break;
        }
        case STATE_UPDATE_LANG: {
            ProcUI::SetHomeButtonMenuEnabled(false); // avoid funny situatuions

            if (!flashUtils.CaffeineInvalidate()) {
                mErrorString = "Failed to invalidate caffeine.";
                mState = STATE_ERROR;
                break;  
            }

            // Abort any potential pending software updates
            CCRCDCSoftwareAbort(CCR_CDC_DESTINATION_DRC0); // avoid funny situations (this shouldnt matter afaict)

            // Reattach the DRC in update mode
            if (!flashUtils.ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_FWUPDATE, FALSE)) {
                flashUtils.ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_ACTIVE, FALSE);
                mErrorString = "Failed to reattach DRC in update mode.";
                mState = STATE_ERROR;
                break;  
            }
            mFlashingProgress = 0;
            mUpdateComplete = false;
            mUpdateResult = 0;
            if (CCRCDCSoftwareLangUpdate(CCR_CDC_DESTINATION_DRC0, mLangPath.c_str(), &targetVersion, SoftwareUpdateCallback, this) != 0) { // Flash language first because it's forgivable somewhat.
                flashUtils.AbortUpdate(CCR_CDC_DESTINATION_DRC0);
                flashUtils.ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_ACTIVE, FALSE);
                mErrorString = "Failed to start language update.";
                mState = STATE_ERROR;
                break;  
            }

            mState = STATE_FLASHING_LANG; // Track progress
            break;
        }
        case STATE_FLASHING_LANG: {
            // Update progress
            CCRCDCFWInfo fwInfo{};
            if (CCRCDCGetFWInfo(CCR_CDC_DESTINATION_DRC0, &fwInfo) == 0) {
                mFlashingProgress = fwInfo.updateProgress;
            }

            OSSleepTicks(OSMillisecondsToTicks(200));

            // Check if update complete
            if (mUpdateComplete) {
                if (mUpdateResult == IOS_ERROR_OK) {
                    mState = STATE_UPDATE_FIRM;
                } else {
                    flashUtils.AbortUpdate(CCR_CDC_DESTINATION_DRC0);
                    flashUtils.ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_ACTIVE, FALSE);
                    mErrorString = "Software update failed.";
                    mState = STATE_ERROR;
                }
            }
            break;
        }
        case STATE_UPDATE_FIRM: {
            ProcUI::SetHomeButtonMenuEnabled(false); // keep avoiding funnies in case there are any.
            // Hey do you like the idea of making that ErrEula "No HOME" popup?
            if (!flashUtils.CaffeineInvalidate()) {
                mErrorString = "Failed to invalidate caffeine.";
                mState = STATE_ERROR;
                break;  
            }

            mFlashingProgress = 0;
            mUpdateComplete = false;
            mUpdateResult = 0;
            if (CCRCDCSoftwareUpdate(CCR_CDC_DESTINATION_DRC0, mFirmwarePath.c_str(), SoftwareUpdateCallback, this) != 0) { // Now flash firmware.
                flashUtils.AbortUpdate(CCR_CDC_DESTINATION_DRC0);
                flashUtils.ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_ACTIVE, FALSE);
                mErrorString = "Failed to start software update.";
                mState = STATE_ERROR;
                break;  
            }

            mState = STATE_FLASHING_FIRM; // Track progress
            break;
        }
        case STATE_FLASHING_FIRM: {
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
                    flashUtils.AbortUpdate(CCR_CDC_DESTINATION_DRC0);
                    flashUtils.ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_ACTIVE, FALSE);
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
            if (langActivateSuccess != 0 || CCRCDCSoftwareActivate(CCR_CDC_DESTINATION_DRC0) != 0) {
                flashUtils.AbortUpdate(CCR_CDC_DESTINATION_DRC0);
                flashUtils.ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_ACTIVE, FALSE);
                mErrorString = "Failed to activate one of the updates.";
                mState = STATE_ERROR;
                break;  
            }

            // Put the gamepad back into active mode
            OSTime startTime = OSGetSystemTime();
            while (!flashUtils.ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_ACTIVE, FALSE)) {
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

void DrcFullFlashScreen::OnUpdateCompleted(int32_t result)
{
    mUpdateComplete = true;
    mUpdateResult = result;
}
