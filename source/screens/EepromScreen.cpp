#include "EepromScreen.hpp"
#include "Gfx.hpp"
#include "ProcUI.hpp"
#include "Utils.hpp"
#include <cstdio>

#include <coreinit/mcp.h>
#include <coreinit/thread.h>
#include <nsysccr/cdc.h>
#include <nsysccr/cfg.h>
#include <nn/ccr.h>

namespace {

bool WriteFile(const uint8_t *source, const std::string& dstPath)
{
    FILE* outf = fopen(dstPath.c_str(), "wb");
    if (!outf) {
        return false;
    }
        if (fwrite(source, 1, 4096, outf) != 0) { // DRC EEPROM size up until "DEADBABE" string appears to be 5E0h bytes, unless you dump the 2nd DRC. What the hell does it add..?
            fclose(outf);
            return false;
        }

    fclose(outf);
    return true;
}

}

EepromScreen::EepromScreen(){}
EepromScreen::~EepromScreen(){}
void EepromScreen::Draw()
{
    DrawTopBar("EepromScreen");

    switch (mState)
    {
        case STATE_DUMP: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT, "Reading data", Gfx::ALIGN_CENTER);
            break;
        }
        case STATE_DONE: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT,
                Utils::sprintf("Saved to eeprom.bin\nPress B"),
                Gfx::ALIGN_CENTER);
            break;
        }
        case STATE_ERROR: {
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_ERROR, "Error:\n" + mErrorString, Gfx::ALIGN_CENTER);
            break;
        }
    }
    if (mState == STATE_ERROR || STATE_DONE) 
        DrawBottomBar(nullptr, nullptr, "\ue001 Back");
}

bool EepromScreen::Update(VPADStatus& input) // This is the core logic part
{
    switch (mState)
    {
        case STATE_DUMP: {
            CCRCDCEepromData readout0;
            uint8_t readoutarray0[0x304];
            CCRCDCEepromData readout1; // If a 2nd DRC is present, we will write this
            uint8_t readoutarray1[0x304];
            // Read EEPROM
            if(CCRCDCPerGetUicEeprom(CCR_CDC_DESTINATION_DRC0, &readout0) != 0){
            	mErrorString = "Read failed!";
                mState = STATE_ERROR;
                break;
            }
            // Write to SD card
            // Convert to uint8_t so that this can be processed by fwrite
            uint8_t versionarray0[4];
            for (int i=0; i<4 ;++i)
                versionarray0[i] = ((uint8_t*)&readout0.version)[3-i];
            memcpy(&readoutarray0[0], versionarray0, 4);
            memcpy(&readoutarray0[4], readout0.data, 0x300);
            WriteFile(readoutarray0, "/vol/external01/eeprom0.bin");
            // If we read the DRC1 EEPROM
            if(CCRCDCPerGetUicEeprom(CCR_CDC_DESTINATION_DRC1, &readout1) == 0){
            	uint8_t versionarray1[4];
                for (int i=0; i<4 ;++i)
                    versionarray1[i] = ((uint8_t*)&readout1.version)[3-i];
                memcpy(&readoutarray1[0], versionarray1, 4);
                memcpy(&readoutarray1[4], readout1.data, 0x300);
                WriteFile(readoutarray1, "/vol/external01/eeprom1.bin");
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

void EepromScreen::OnDumpCompleted()
{
    mDumpComplete = true;
}
