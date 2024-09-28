#include "FormatScreen.hpp"
#include "Gfx.hpp"
#include "ProcUI.hpp"
#include "Utils.hpp"
#include <cstdio>

#include <nsysccr/cdc.h>
#include <sysapp/launch.h>
#include <nn/ccr.h>

namespace {}

FormatScreen::FormatScreen(){
    CCRCDCSetMultiDrc(1); // Having multiple DRCs enabled will make it impossible to erase a gamepad.
}
FormatScreen::~FormatScreen(){}
void FormatScreen::Draw()
{
    DrawTopBar("FormatScreen");

    switch (mState)
    {
        case STATE_UPDATE: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT, "Resetting data", Gfx::ALIGN_CENTER);
            break;
        }
        case STATE_DONE: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT,
                Utils::sprintf("Done!"),
                Gfx::ALIGN_CENTER);
            break;
        }
        case STATE_ERROR: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_ERROR, "Error:\n" + mErrorString, Gfx::ALIGN_CENTER);
            break;
        }
    }
}

bool FormatScreen::Update(VPADStatus& input) // This is the core logic part
{
    switch (mState)
    {
        case STATE_UPDATE: {
            ProcUI::SetHomeButtonMenuEnabled(false);


            // Abort any potential pending software updates
            CCRCDCSoftwareAbort(CCR_CDC_DESTINATION_DRC0);

            // Erase DRC
            if(CCRSysInitializeSettings() != 0){
            	mErrorString = "Erase failed.";
                mState = STATE_ERROR;
                break;
            }
            mState = STATE_DONE;
            break;
        }
        case STATE_DONE: {
            CCRCDCSysConsoleShutdownInd(CCR_CDC_DESTINATION_DRC0); // Power off device to apply changes
            SYSLaunchMenu();
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