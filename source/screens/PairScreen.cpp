#include "PairScreen.hpp"
#include "Gfx.hpp"
#include "ProcUI.hpp"
#include "Utils.hpp"
#include <cstdio>

#include <nsysccr/cdc.h>
#include <nsysccr/cfg.h>
#include <nn/ccr.h>
#include <malloc.h>

#define TIMEOUT_SECONDS 45
namespace {}

PairScreen::PairScreen():mTargetEntries({{DRC0, {0x31, "Main DRC"}}, {DRC1, {0x32, "Additional DRC"}},}){
    
    // Initialize IM
    imHandle = IM_Open();
    imRequest = (IMRequest*) memalign(0x40, sizeof(IMRequest));
    // Allocate a separate request for IM_CancelGetEventNotify to avoid conflict with the pending IM_GetEventNotify request
    imCancelRequest = (IMRequest*) memalign(0x40, sizeof(IMRequest));

    // Init CCRSys
    CCRSysInit();
}
PairScreen::~PairScreen(){
    CCRSysExit();

    // Close IM
    IM_CancelGetEventNotify(imHandle, imCancelRequest, nullptr, nullptr);
    IM_Close(imHandle);
    free(imCancelRequest);
    free(imRequest);
}
void PairScreen::Draw()
{
    DrawTopBar("PairScreen");

    switch (mState)
    {
        case STATE_SELECT_TARGET: {
            for (TargetID id = DRC0; id <= DRC1; id = static_cast<TargetID>(id + 1)) {
                int yOff = 75 + static_cast<int>(id) * 150;
                Gfx::DrawRectFilled(0, yOff, Gfx::SCREEN_WIDTH, 150, Gfx::COLOR_ALT_BACKGROUND);
                Gfx::DrawIcon(68, yOff + 150 / 2, 60, Gfx::COLOR_TEXT, mTargetEntries[id].icon);
                Gfx::Print(128 + 8, yOff + 150 / 2, 60, Gfx::COLOR_TEXT, mTargetEntries[id].name, Gfx::ALIGN_VERTICAL);

                if (id == mTarget) {
                    Gfx::DrawRect(0, yOff, Gfx::SCREEN_WIDTH, 150, 8, Gfx::COLOR_HIGHLIGHTED);
                }
            }
            break;
        }
        case STATE_GETPIN: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT, "Getting pincode...", Gfx::ALIGN_CENTER);
            break;
        }
        case STATE_WAITING: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT, 
            Utils::sprintf("Got WPS PIN! \n"
                               "Pincode: %i\n"
                               "GamePad keyboard: 0213", pincodebuffer), Gfx::ALIGN_CENTER);
	    break;
        }
        case STATE_DONE: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT,
                Utils::sprintf("DRC paired successfully"),
                Gfx::ALIGN_CENTER);
            break;
        }
        case STATE_ERROR: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_ERROR, "Error:\n" + mErrorString, Gfx::ALIGN_CENTER);
            break;
        }
    }
    if (mState == STATE_SELECT_TARGET) {
        DrawBottomBar("\ue07d Navigate", "\ue044 Exit", "\ue000 Confirm / \ue001 Back");
    } else if (mState == STATE_GETPIN) {
        DrawBottomBar(nullptr, "Please wait...", nullptr);
    } else if (mState == STATE_WAITING) {
        DrawBottomBar(nullptr, "Press SYNC to cancel pairing", nullptr);
    } else {
        DrawBottomBar(nullptr, nullptr, "\ue001 Back");
    }
}

bool PairScreen::Update(VPADStatus& input) // This is the core logic part
{
    switch (mState)
    {
        case STATE_SELECT_TARGET: {
            if (input.trigger & VPAD_BUTTON_B) {
                return false;
            }

            if (input.trigger & VPAD_BUTTON_A) {
                mState = STATE_GETPIN;
                break;
            }

            if (input.trigger & VPAD_BUTTON_DOWN) {
                if (mTarget < DRC1) {
                    mTarget = static_cast<TargetID>(mTarget + 1);
                }
            } else if (input.trigger & VPAD_BUTTON_UP) {
                if (mTarget > DRC0) {
                    mTarget = static_cast<TargetID>(mTarget - 1);
                }
            }
            break;
        }
        case STATE_GETPIN: {
            cancelPairing = false;
            imEventMask = IM_EVENT_SYNC;
            IM_GetEventNotify(imHandle, imRequest, &imEventMask, PairScreen::SyncButtonCallback, &imEventMask);
            // Get a PIN code to display.
            if (CCRSysGetPincode(&pincodebuffer) != 0){
            	mErrorString = "Could not obtain the PIN!";
                mState = STATE_ERROR;
                break;
            }
            switch (mTarget) {
                case DRC0: {
                    if (CCRSysStartPairing(0, TIMEOUT_SECONDS) != 0) {
                        mErrorString = "Could not start pairing!";
                        mState = STATE_ERROR;
                        break;
                    }
                    mState = STATE_WAITING;
                    break;
                }
                case DRC1: {
                    if (CCRCDCSetMultiDrc(2) != 0) {
                        mErrorString = "Could not enable MultiDRC mode!";
                        mState = STATE_ERROR;
                        break;
                    }
                    if (CCRSysStartPairing(1, TIMEOUT_SECONDS) != 0) {
                        mErrorString = "Could not start pairing!";
                        mState = STATE_ERROR;
                        break;
                    }
                    mState = STATE_WAITING;
                    break;
                }
            }
        }
        case STATE_WAITING: {
            CCRSysPairingState pairingState = CCRSysGetPairingState();
            if (pairingState == CCR_SYS_PAIRING_TIMED_OUT || cancelPairing) {
                // Pairing has timed out or was cancelled
                CCRSysStopPairing();
                mErrorString = "Pairing was stopped!";
                mState = STATE_ERROR;
                break;
            } else if (pairingState == CCR_SYS_PAIRING_FINISHED) {
                // DRC was paired
                mState = STATE_DONE;
            }
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

void PairScreen::SyncButtonCallback(IOSError error, void* arg)
{
    uint32_t event = *(uint32_t*) arg;

    // Cancel pairing if the sync button was pressed
    if (error == IOS_ERROR_OK && (event & IM_EVENT_SYNC)) {
        cancelPairing = true;
    }
}

void PairScreen::OnPairingCompleted()
{
    mPairingComplete = true;
}
