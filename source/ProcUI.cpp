#include "ProcUI.hpp"
#include <coreinit/foreground.h>
#include <coreinit/title.h>
#include <proc_ui/procui.h>
#include <sysapp/launch.h>

#include <nn/erreula.h> // no HOME icon
#include <vpad/input.h> // DRC input
#include <padscore/kpad.h> // BT input

namespace {

constexpr uint64_t kMiiMakerTitleID = 0x000500101004A000ull;
constexpr uint64_t kHBLTitleID = 0x0005000013374842;

bool isRunning = true;
bool isLegacyLoader = false;
bool isHomeButtonMenuEnabled = true;

inline bool RunningFromLegacySetup()
{
    uint64_t titleID = OSGetTitleID();

    // Mask the region bits when comparing
    return (titleID & 0xFFFFFFFFFFFFF0FFull) == kMiiMakerTitleID || titleID == kHBLTitleID;
}

uint32_t SaveCallback(void* context)
{
    OSSavesDone_ReadyToRelease();
    return 0;
}

uint32_t HomeButtonDeniedCallback(void* context)
{
    if (!isHomeButtonMenuEnabled) {
        VPADStatus vpadStatus;
        UpdatePads(vpadStatus);
        nn::erreula::HomeNixSignArg noHOME;
        noHOME.unknown0x00 = 1;
        if (vpadStatus.trigger & 0x2) {AppearHomeNixSign(noHOME);} // have you pushed HOME?
    } else {
        if (isLegacyLoader) {
            ProcUI::StopRunning();
        }
    }

    return 0;
}

}

void ProcUI::Init()
{
    // Check if we're running from a legacy setup
    if (RunningFromLegacySetup()) {
        isLegacyLoader = true;
        OSEnableHomeButtonMenu(FALSE);
    }

    isRunning = true;

    ProcUIInitEx(SaveCallback, nullptr);
    ProcUIRegisterCallback(PROCUI_CALLBACK_HOME_BUTTON_DENIED, HomeButtonDeniedCallback, NULL, 100);
}

void ProcUI::Shutdown()
{
    isRunning = false;

    // Legacy loaders require a title relaunch
    if (isLegacyLoader) {
        SYSRelaunchTitle(0, NULL);
    }
}

bool ProcUI::IsRunning()
{
    ProcUIStatus status = ProcUIProcessMessages(TRUE);
    if (status == PROCUI_STATUS_EXITING) {
        isRunning = false;
    } else if (status == PROCUI_STATUS_RELEASE_FOREGROUND) {
        ProcUIDrawDoneRelease();
    }

    if (!isRunning) {
        ProcUIShutdown();
    }

    return isRunning;
}

void ProcUI::StopRunning()
{
    // Legacy loaders can just return from main loop, otherwise we need a title to boot into
    if (isLegacyLoader) {
        isRunning = false;
    } else {
        SYSLaunchMenu();
    }
}

void ProcUI::SetHomeButtonMenuEnabled(bool enabled)
{
    isHomeButtonMenuEnabled = enabled;

    if (!isLegacyLoader) {
        OSEnableHomeButtonMenu(enabled);
    }
}


