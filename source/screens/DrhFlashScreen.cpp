#include "DrhFlashScreen.hpp"
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
#include <coreinit/launch.h>

#define OS_TITLE_ID_REBOOT 0xFFFFFFFFFFFFFFFEllu

namespace {
/*
bool ReadFirmwareHeader(const std::string& path, DrhFlashScreen::FirmwareHeader& header)
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
    CCRCDCSoftwareGetVersion(CCR_CDC_DESTINATION_DRH, &version);

    // Only newer versions have caffeine
    if (version.runningVersion >= 0x180a0000) {
        return CCRSysCaffeineSetCaffeineSlot(0xff) == 0;
    }

    return true;
}

bool ReattachDRH(CCRCDCDrhStateEnum targetState, BOOL unknown)
{
    // Get the current DRC state
    CCRCDCDrhState state;
    int32_t res = CCRCDCSysGetDrhState(&state);
    if (res != 0) {
        return false;
    }

    // Nothing to do if we're already in the target state
    if (state.state == targetState) {
        return true;
    }

    // Set target state
    state.state = targetState;
    res = CCRCDCSysSetDrhState(&state);
    if (res != 0) {
        return false;
    }

    // Check if we're in the state we want
    res = CCRCDCSysGetDrhState(&state);
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
}*/

void SoftwareUpdateCallback(IOSError error, void* arg)
{
    DrhFlashScreen* drhFlashScreen = static_cast<DrhFlashScreen*>(arg);

    drhFlashScreen->OnUpdateCompleted(error);
}

}

DrhFlashScreen::DrhFlashScreen()
{
}

DrhFlashScreen::~DrhFlashScreen()
{
}

void DrhFlashScreen::Draw()
{
    DrawTopBar("DrhFlashScreen");

    switch (mState)
    {
        case STATE_PREPARE: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT, "Preparing...", Gfx::ALIGN_CENTER);
            break;
        }
        case STATE_CONFIRM2: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT,
                Utils::sprintf("DRH update is ready.\n"
                               "About to flash firmware version 0x%08x.\n"
                               "Would you like to start the update now?\n", mFirmwareHeader.version),
                Gfx::ALIGN_CENTER);
            break;
        }
        case STATE_UPDATE: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT, "Starting update...", Gfx::ALIGN_CENTER);
            break;
        }
        case STATE_FLASHING: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2 - 32, 64, Gfx::COLOR_TEXT, Utils::sprintf("Updating... %d%%", mFlashingProgress), Gfx::ALIGN_CENTER);
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
                Utils::sprintf("The update is complete.\n"
                               "The console will now restart."),
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
    } else if (mState == STATE_DONE) {
        DrawBottomBar(nullptr, nullptr, "\ue000 Reboot");
    } else if (mState == STATE_ERROR) {
        DrawBottomBar(nullptr, nullptr, "\ue001 Back");
    }
}

bool DrhFlashScreen::Update(VPADStatus& input)
{
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
            if (!flashUtils.ReadFirmwareHeader("/vol/external01/drh_fw.bin", mFirmwareHeader)) {
                mErrorString = "Failed to read DRH firmware header";
                mState = STATE_ERROR;
                break;
            }
                // Copy to MLC so IOS-PAD can install it
            mFirmwarePath = "/vol/storage_mlc01/usr/tmp/drh_fw.bin";
            if (!flashUtils.CopyFile("/vol/external01/drh_fw.bin", "storage_mlc01:/usr/tmp/drh_fw.bin")) {
                mErrorString = "Failed to copy firmware to MLC";
                mState = STATE_ERROR;
                break;
            }
            mState = STATE_CONFIRM2;
            break;
        }
        case STATE_UPDATE: {
            ProcUI::SetHomeButtonMenuEnabled(false);

            if (!flashUtils.CaffeineInvalidate()) {
                mErrorString = "Failed to invalidate caffeine.";
                mState = STATE_ERROR;
                break;  
            }

            // Abort any potential pending software updates
            CCRCDCSoftwareAbort(CCR_CDC_DESTINATION_DRH);

            mFlashingProgress = 0;
            mUpdateComplete = false;
            mUpdateResult = 0;
            if (CCRCDCSoftwareUpdate(CCR_CDC_DESTINATION_DRH, mFirmwarePath.c_str(), SoftwareUpdateCallback, this) != 0) {
                flashUtils.AbortUpdate(CCR_CDC_DESTINATION_DRH);
                flashUtils.ReattachDRH(CCR_CDC_SYS_DRH_STATE_CAFE, FALSE);
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
            if (CCRCDCGetFWInfo(CCR_CDC_DESTINATION_DRH, &fwInfo) == 0) {
                mFlashingProgress = fwInfo.updateProgress;
            }

            OSSleepTicks(OSMillisecondsToTicks(200));

            // Check if update complete
            if (mUpdateComplete) {
                if (mUpdateResult == IOS_ERROR_OK) {
                    mState = STATE_ACTIVATE;
                } else {
                    flashUtils.AbortUpdate(CCR_CDC_DESTINATION_DRH);
                    flashUtils.ReattachDRH(CCR_CDC_SYS_DRH_STATE_CAFE, FALSE);
                    mErrorString = "Software update failed - error " + std::to_string(mUpdateResult);
                    mState = STATE_ERROR;
                }
            }
            break;
        }
        case STATE_ACTIVATE: {
            // Activate the newly flashed firmware
            if (CCRCDCSoftwareActivate(CCR_CDC_DESTINATION_DRH) != 0) {
                flashUtils.AbortUpdate(CCR_CDC_DESTINATION_DRH);
                flashUtils.ReattachDRH(CCR_CDC_SYS_DRH_STATE_CAFE, FALSE);
                mErrorString = "Failed to activate software update.";
                mState = STATE_ERROR;
                break;  
            }

            // Put the DRH back into active mode
            OSTime startTime = OSGetSystemTime();
            while (!flashUtils.ReattachDRH(CCR_CDC_SYS_DRH_STATE_CAFE, FALSE)) {
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
            if (input.trigger & VPAD_BUTTON_A) {
                // Reboot system to apply changes
                ProcUI::SetHomeButtonMenuEnabled(true);
                OSLaunchTitlev(OS_TITLE_ID_REBOOT, 0, NULL);
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

void DrhFlashScreen::OnUpdateCompleted(int32_t result)
{
    mUpdateComplete = true;
    mUpdateResult = result;
}
