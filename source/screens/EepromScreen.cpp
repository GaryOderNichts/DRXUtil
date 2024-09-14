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

bool WriteFile(const void *source, const std::string& dstPath)
{
    FILE* outf = fopen(dstPath.c_str(), "wb");
    if (!outf) {
        return false;
    }
    // Developer observations:
    // DRC EEPROM size up until "DEADBABE" string appears to be 5E0h bytes. The DK menu says it is 600h bytes.
    // The real EEPROM size should be 304h bytes.
    // If you dump the 2nd DRC, for some reason data for the 1st DRC will be appended too.
    // For our needs hardcoding the size to 304h bytes is going to be sufficient.
    if (fwrite(source, 1, 0x304, outf) != 0) { 
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
                Utils::sprintf(
                    "Saved to eeprom0.bin",
                    drc1_read ? " and eeprom1.bin\n" : "\n",
                    "Press B"
                ),
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
            CCRCDCEepromData readout1; // If a 2nd DRC is present, we will write this
            // Read EEPROM
            if(CCRCDCPerGetUicEeprom(CCR_CDC_DESTINATION_DRC0, &readout0)){
            	mErrorString = "Read failed!";
                mState = STATE_ERROR;
                break;
            }
            // Write to SD card 
            WriteFile(&readout0, "/vol/external01/eeprom0.bin");
            // If we read the DRC1 EEPROM
            if(CCRCDCPerGetUicEeprom(CCR_CDC_DESTINATION_DRC1, &readout1) == 0){
                drc1_read = true;
            	//uint8_t versionarray1[4];
                //for (int i=0; i<4 ;++i)
                //    versionarray1[i] = ((uint8_t*)&readout1.version)[3-i];
                //memcpy(&readoutarray1[0], versionarray1, 4);
                //memcpy(&readoutarray1[4], readout1.data, 0x300);
                WriteFile(&readout1, "/vol/external01/eeprom1.bin");
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
