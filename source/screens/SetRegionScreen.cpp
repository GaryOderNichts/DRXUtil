#include "SetRegionScreen.hpp"
#include "Utils.hpp"
#include "Gfx.hpp"

#include <nsysccr/cdc.h>
#include <nsysccr/cfg.h>

namespace {

bool SetRegionByte(uint8_t byte)
{
    CCRCDCUicConfig cfg{};
    // custom config id which was added by the gamepad cfw
    cfg.configId = 1;
    // region byte + crc16
    cfg.size = 3;
    cfg.data[0] = byte;
    uint16_t crc = CCRCDCCalcCRC16(cfg.data, 1);
    cfg.data[1] = crc & 0xff;
    cfg.data[2] = (crc >> 8) & 0xff;
    if (CCRCDCPerSetUicConfig(CCR_CDC_DESTINATION_DRC0, &cfg) != 0) {
        return false;
    }

    // Also update the cached eeprom
    return CCRCFGSetCachedEeprom(0, 0x103, cfg.data, cfg.size) == 0;
}

}

SetRegionScreen::SetRegionScreen()
 : mRegionEntries({
    { REGION_JAPAN,       "JAPAN" },
    { REGION_AMERICA,     "AMERICA" },
    { REGION_EUROPE,      "EUROPE" },
    { REGION_CHINA,       "CHINA" },
    { REGION_SOUTH_KOREA, "SOUTH KOREA" },
    { REGION_TAIWAN,      "TAIWAN" },
    { REGION_AUSTRALIA,   "AUSTRALIA" },
 })
{
}

SetRegionScreen::~SetRegionScreen()
{
}

void SetRegionScreen::Draw()
{
    DrawTopBar("SetRegionScreen");

    switch (mState)
    {
        case STATE_SELECT_REGION:
            for (Region id = REGION_JAPAN; id <= REGION_AUSTRALIA; id = static_cast<Region>(id + 1)) {
                int yOff = 75 + static_cast<int>(id) * 100;
                Gfx::DrawRectFilled(0, yOff, Gfx::SCREEN_WIDTH, 100, Gfx::COLOR_ALT_BACKGROUND);
                Gfx::Print(68, yOff + 100 / 2, 50, Gfx::COLOR_TEXT, mRegionEntries[id], Gfx::ALIGN_VERTICAL);

                if (id == mRegion) {
                    Gfx::DrawRect(0, yOff, Gfx::SCREEN_WIDTH, 100, 8, Gfx::COLOR_HIGHLIGHTED);
                }
            }
            break;
        case STATE_CONFIRM:
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT,
                Utils::sprintf("Are you sure you want to set the region\nto %s?\n"
                               "(Note that updating the region\nonly works with a modified firmware)",
                mRegionEntries[mRegion].c_str()), Gfx::ALIGN_CENTER);
            break;
        case STATE_UPDATE:
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT, "Updating region...", Gfx::ALIGN_CENTER);
            break;
        case STATE_DONE:
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT, "Done! Region has been changed.", Gfx::ALIGN_CENTER);
            break;
        case STATE_ERROR:
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_ERROR, "Error! Failed to set region.\n(Is the firmware modified properly?)", Gfx::ALIGN_CENTER);
            break;
    }

    if (mState == STATE_SELECT_REGION) {
        DrawBottomBar("\ue07d Navigate", "\ue044 Exit", "\ue000 Confirm / \ue001 Back");
    } else if (mState == STATE_CONFIRM) {
        DrawBottomBar(nullptr, "\ue044 Exit", "\ue000 Confirm / \ue001 Back");
    } else if (mState == STATE_UPDATE) {
        DrawBottomBar(nullptr, "Please wait...", nullptr);
    } else {
        DrawBottomBar(nullptr, "\ue044 Exit", "\ue001 Back");
    }
}

bool SetRegionScreen::Update(VPADStatus& input)
{
    switch (mState)
    {
        case STATE_SELECT_REGION:
            if (input.trigger & VPAD_BUTTON_B) {
                return false;
            }

            if (input.trigger & VPAD_BUTTON_A) {
                mState = STATE_CONFIRM;
                break;
            }

            if (input.trigger & VPAD_BUTTON_DOWN) {
                if (mRegion < REGION_AUSTRALIA) {
                    mRegion = static_cast<Region>(mRegion + 1);
                }
            } else if (input.trigger & VPAD_BUTTON_UP) {
                if (mRegion > REGION_JAPAN) {
                    mRegion = static_cast<Region>(mRegion - 1);
                }
            }
            break;
        case STATE_CONFIRM:
            if (input.trigger & VPAD_BUTTON_B) {
                return false;
            }

            if (input.trigger & VPAD_BUTTON_A) {
                mState = STATE_UPDATE;
                break;
            }
            break;
        case STATE_UPDATE:
            if (!SetRegionByte(static_cast<uint8_t>(mRegion))) {
                mState = STATE_ERROR;
                break;
            }
            
            mState = STATE_DONE;
            break;
        case STATE_DONE:
        case STATE_ERROR:
            if (input.trigger & VPAD_BUTTON_B) {
                return false;
            }
            break;
    }

    return true;
}
