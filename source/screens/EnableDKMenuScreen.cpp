#include "EnableDKMenuScreen.hpp"
#include "Utils.hpp"
#include "Gfx.hpp"

#include <nsysccr/cdc.h>
#include <nsysccr/cfg.h>

namespace {

bool SetBoardConfig(uint8_t byte)
{
    CCRCDCUicConfig cfg{};
    // custom config id which was added by the gamepad cfw
    cfg.configId = 4;
    // board config byte + crc16
    cfg.size = 3;
    cfg.data[0] = byte;
    uint16_t crc = CCRCDCCalcCRC16(cfg.data, 1);
    cfg.data[1] = crc & 0xff;
    cfg.data[2] = (crc >> 8) & 0xff;
    if (CCRCDCPerSetUicConfig(CCR_CDC_DESTINATION_DRC0, &cfg) != 0) {
        return false;
    }

    // Also update the cached eeprom
    return CCRCFGSetCachedEeprom(0, 0x106, cfg.data, cfg.size) == 0;
}

bool GetBoardConfig(uint8_t& boardConfig)
{
    uint8_t data[3];
    if (CCRCFGGetCachedEeprom(0, 0x106, data, 3) != 0) {
        return false;
    }

    uint16_t crc = (uint16_t) data[2] << 8 | data[1];
    if (CCRCDCCalcCRC16(&data[0], 1) != crc) {
        return false;
    }

    boardConfig = data[0];
    return true;
}

}

EnableDKMenuScreen::EnableDKMenuScreen()
 : mConfirm(false),
   mBoardConfig(0),
   mDKMenuEnabled(false),
   mErrorText()
{
}

EnableDKMenuScreen::~EnableDKMenuScreen()
{
}

void EnableDKMenuScreen::Draw()
{
    DrawTopBar("EnableDKMenuScreen");

    switch (mState)
    {
        case STATE_GET_BOARD_CONFIG:
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT, "Retrieving board config...", Gfx::ALIGN_CENTER);
            break;
        case STATE_SELECT:
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, 128, 64, Gfx::COLOR_TEXT,
                Utils::sprintf("DK Menu is currently %s.\nDo you want to %s it?",
                               mDKMenuEnabled ? "enabled" : "disabled",
                               mDKMenuEnabled ? "disable" : "enable"),
                Gfx::ALIGN_HORIZONTAL | Gfx::ALIGN_TOP);
            for (int i = 0; i < 2; i++) {
                int yOff = 360 + static_cast<int>(i) * 100;
                Gfx::DrawRectFilled(0, yOff, Gfx::SCREEN_WIDTH, 100, Gfx::COLOR_ALT_BACKGROUND);
                Gfx::Print(68, yOff + 100 / 2, 50, Gfx::COLOR_TEXT, i == 0 ? "Back" : (mDKMenuEnabled ? "Disable" : "Enable"), Gfx::ALIGN_VERTICAL);

                if (mConfirm == !!i) {
                    Gfx::DrawRect(0, yOff, Gfx::SCREEN_WIDTH, 100, 8, Gfx::COLOR_HIGHLIGHTED);
                }
            }
            break;
        case STATE_CONFIRM:
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT,
                Utils::sprintf("Are you sure you want to %s the DK Menu?\n"
                               "(Note that updating the board config\nonly works with a modified firmware)",
                mDKMenuEnabled ? "disable" : "enable"), Gfx::ALIGN_CENTER);
            break;
        case STATE_UPDATE:
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT, "Updating board config...", Gfx::ALIGN_CENTER);
            break;
        case STATE_DONE:
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT,
                Utils::sprintf("Done! DK Menu has been %s.\n%s",
                                mDKMenuEnabled ? "disabled" : "enabled",
                                mDKMenuEnabled ? "" : "(Hold L + ZL while powering on the Gamepad to open.)"),
                Gfx::ALIGN_CENTER);
            break;
        case STATE_ERROR:
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_ERROR, "Error!\n" + mErrorText, Gfx::ALIGN_CENTER);
            break;
    }

    if (mState == STATE_SELECT) {
        DrawBottomBar("\ue07d Navigate", "\ue044 Exit", "\ue000 Confirm / \ue001 Back");
    } else if (mState == STATE_CONFIRM) {
        DrawBottomBar(nullptr, "\ue044 Exit", "\ue000 Confirm / \ue001 Back");
    } else if (mState == STATE_UPDATE || mState == STATE_GET_BOARD_CONFIG) {
        DrawBottomBar(nullptr, "Please wait...", nullptr);
    } else {
        DrawBottomBar(nullptr, "\ue044 Exit", "\ue001 Back");
    }
}

bool EnableDKMenuScreen::Update(VPADStatus& input)
{
    switch (mState)
    {
        case STATE_GET_BOARD_CONFIG:
            if (!GetBoardConfig(mBoardConfig)) {
                mErrorText = "Failed to retrieve current board configuration.";
                mState = STATE_ERROR;
                break;
            }
            mDKMenuEnabled = (mBoardConfig & 0x0C) == 0;
            mState = STATE_SELECT;
            break;
        case STATE_SELECT:
            if (input.trigger & VPAD_BUTTON_B) {
                return false;
            }

            if (input.trigger & VPAD_BUTTON_A) {
                if (mConfirm) {
                    mState = STATE_CONFIRM;
                } else {
                    return false;
                }
                break;
            }

            if (input.trigger & (VPAD_BUTTON_DOWN | VPAD_BUTTON_UP)) {
                mConfirm = !mConfirm;
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
        case STATE_UPDATE: {
            uint8_t newBoardConfig = mBoardConfig;
            // Clear/Set the is retail bits
            if (mDKMenuEnabled) {
                newBoardConfig |= 0x04;
            } else {
                newBoardConfig &= ~0x0C;
            }

            if (!SetBoardConfig(newBoardConfig)) {
                mErrorText = "Failed to set board config.\n(Is the firmware modified properly?)";
                mState = STATE_ERROR;
                break;
            }

            mState = STATE_DONE;
            break;
        }
        case STATE_DONE:
        case STATE_ERROR:
            if (input.trigger & VPAD_BUTTON_B) {
                return false;
            }
            break;
    }

    return true;
}