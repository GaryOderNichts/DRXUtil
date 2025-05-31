#include "EditDeviceInfoScreen.hpp"
#include "Utils.hpp"
#include "Gfx.hpp"

#include <nsysccr/cdc.h>
#include <nsysccr/cfg.h>

namespace {

bool SetDeviceInfo(uint8_t byte)
{
    CCRCDCUicConfig cfg{};
    // custom config id which was added by the gamepad cfw
    cfg.configId = 4;
    // device info byte + crc16
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

bool GetDeviceInfo(uint8_t& deviceInfo)
{
    uint8_t data[3];
    if (CCRCFGGetCachedEeprom(0, 0x106, data, 3) != 0) {
        return false;
    }

    uint16_t crc = (uint16_t) data[2] << 8 | data[1];
    if (CCRCDCCalcCRC16(&data[0], 1) != crc) {
        return false;
    }

    deviceInfo = data[0];
    return true;
}

}

EditDeviceInfoScreen::EditDeviceInfoScreen()
 : mSelected(OPTION_ID_MIN),
   mDeviceInfo(0),
   mIsDevelopment(false),
   mIsUnshipped(false),
   mHasChanged(false),
   mIsApplying(false),
   mErrorText(),
   mMessageBox()
{
}

EditDeviceInfoScreen::~EditDeviceInfoScreen()
{
}

void EditDeviceInfoScreen::Draw()
{
    DrawTopBar("EditDeviceInfoScreen");

    switch (mState)
    {
        case STATE_GET_DEVICE_INFO:
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT, "Retrieving device info...", Gfx::ALIGN_CENTER);
            break;
        case STATE_SELECT: {
            int yOff = 75;

            // OPTION_ID_PRODUCTION_TYPE
            Gfx::DrawRectFilled(0, yOff, Gfx::SCREEN_WIDTH, 150, Gfx::COLOR_ALT_BACKGROUND);
            Gfx::Print(128 + 8, yOff + 150 / 2, 60, Gfx::COLOR_TEXT, "Production Type", Gfx::ALIGN_VERTICAL);
            if (mIsDevelopment) {
                Gfx::Print(Gfx::SCREEN_WIDTH - (128 + 8), yOff + 150 / 2, 60, Gfx::COLOR_TEXT, "< Development >", Gfx::ALIGN_VERTICAL | Gfx::ALIGN_RIGHT);
            } else {
                Gfx::Print(Gfx::SCREEN_WIDTH - (128 + 8), yOff + 150 / 2, 60, Gfx::COLOR_TEXT, "< Mass >", Gfx::ALIGN_VERTICAL | Gfx::ALIGN_RIGHT);
            }
            yOff += 150;

            // OPTION_ID_SHIPMENT_STATUS
            Gfx::DrawRectFilled(0, yOff, Gfx::SCREEN_WIDTH, 150, Gfx::COLOR_ALT_BACKGROUND);
            Gfx::Print(128 + 8, yOff + 150 / 2, 60, Gfx::COLOR_TEXT, "Shipment Status", Gfx::ALIGN_VERTICAL);
            if (mIsUnshipped) {
                Gfx::Print(Gfx::SCREEN_WIDTH - (128 + 8), yOff + 150 / 2, 60, Gfx::COLOR_TEXT, "< Unshipped >", Gfx::ALIGN_VERTICAL | Gfx::ALIGN_RIGHT);
            } else {
                Gfx::Print(Gfx::SCREEN_WIDTH - (128 + 8), yOff + 150 / 2, 60, Gfx::COLOR_TEXT, "< Shipped >", Gfx::ALIGN_VERTICAL | Gfx::ALIGN_RIGHT);
            }
            yOff += 150;

            // OPTION_ID_APPLY_CHANGES
            if (mHasChanged) {
                Gfx::DrawRectFilled(0, yOff, Gfx::SCREEN_WIDTH, 150, Gfx::COLOR_ALT_BACKGROUND);
                Gfx::Print(128 + 8, yOff + 150 / 2, 60, Gfx::COLOR_TEXT, "Apply changes", Gfx::ALIGN_VERTICAL);
                Gfx::Print(Gfx::SCREEN_WIDTH - (128 + 8), yOff + 150 / 2, 60, Gfx::COLOR_TEXT, "\ue000", Gfx::ALIGN_VERTICAL | Gfx::ALIGN_RIGHT);
                yOff += 150;
            }

            // Draw selected indicator
            Gfx::DrawRect(0, 75 + mSelected * 150, Gfx::SCREEN_WIDTH, 150, 8, Gfx::COLOR_HIGHLIGHTED);

            break;
        }
        case STATE_ERROR:
            Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_ERROR, "Error!\n" + mErrorText, Gfx::ALIGN_CENTER);
            break;
    }

    if (mState == STATE_SELECT) {
        if (mSelected == OPTION_ID_APPLY_CHANGES) {
            DrawBottomBar("\ue07d Navigate", "\ue044 Exit", "\ue000 Confirm / \ue001 Back");
        } else {
            DrawBottomBar("\ue07d Navigate", "\ue044 Exit", "\ue07e Modify / \ue001 Back");
        }
    } else if (mState == STATE_GET_DEVICE_INFO) {
        DrawBottomBar(nullptr, "Please wait...", nullptr);
    } else {
        DrawBottomBar(nullptr, "\ue044 Exit", "\ue001 Back");
    }

    if (mMessageBox) {
        mMessageBox->Draw();
    }

    if (mIsApplying) {
        Gfx::DrawRectFilled(0, 0, Gfx::SCREEN_WIDTH, Gfx::SCREEN_WIDTH, { 0, 0, 0, 0xa0 });
        Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, Gfx::COLOR_TEXT, "Updating device info...", Gfx::ALIGN_CENTER);
    }
}

bool EditDeviceInfoScreen::Update(VPADStatus& input)
{
    if (mMessageBox) {
        if (!mMessageBox->Update(input)) {
            mMessageBox.reset();
        }

        return true;
    }

    switch (mState)
    {
        case STATE_GET_DEVICE_INFO:
            if (!GetDeviceInfo(mDeviceInfo)) {
                mErrorText = "Failed to retrieve current device info.";
                mState = STATE_ERROR;
                break;
            }
            mIsDevelopment = (mDeviceInfo & 0x0C) == 0;
            mIsUnshipped = (mDeviceInfo & 0x03) == 0;
            mState = STATE_SELECT;
            break;
        case STATE_SELECT: {
            if (mIsApplying) {
                break;
            }

            if (input.trigger & VPAD_BUTTON_B) {
                return false;
            }

            if (input.trigger & (VPAD_BUTTON_LEFT | VPAD_BUTTON_RIGHT)) {
                switch (mSelected) {
                    case OPTION_ID_PRODUCTION_TYPE:
                        mIsDevelopment = !mIsDevelopment;
                        break;
                    case OPTION_ID_SHIPMENT_STATUS:
                        mIsUnshipped = !mIsUnshipped;
                        break;
                    default: break;
                }

                if (mSelected == OPTION_ID_SHIPMENT_STATUS && mIsUnshipped) {
                    mMessageBox = std::make_unique<MessageBox>(
                        "This option will unship your Gamepad!",
                        "Are you sure you want to set your Gamepad to an unshipped state?\n"
                        "Only do this if you know what you're doing.\n"
                        "If your Gamepad is set to unshipped, it will only boot into the Service Menu.\n"
                        "If your Service Menu doesn't work or is corrupted, your Gamepad will be bricked.\n"
                        "\n"
                        "To get out of the Service Menu navigate to \"UIC Menu\" and select\n"
                        "\"Write Shipping Flag On\" using the \ue000 button. Use the \ue045 button to execute it.\n"
                        "Afterwards, restart the Gamepad.",
                        std::vector{
                            MessageBox::Option{0, "\ue000 OK", [this]() {} },
                        }
                    );
                }

                mHasChanged = true;
            }

            if (input.trigger & VPAD_BUTTON_A) {
                if (mSelected == OPTION_ID_APPLY_CHANGES) {
                    mMessageBox = std::make_unique<MessageBox>(
                        "Are you sure?",
                        "Are you sure you want to update the device info?\n"
                        "Only do this if you know what you're doing.",
                        std::vector{
                            MessageBox::Option{0, "\ue001 Back", [this]() {} },
                            MessageBox::Option{0xf00c, "Update", [this]() {
                                mIsApplying = true;
                            }},
                        }
                    );
                }
                break;
            }

            if (input.trigger & VPAD_BUTTON_DOWN) {
                if (mSelected < (OPTION_ID_MAX - 1) || (mSelected < OPTION_ID_MAX && mHasChanged)) {
                    mSelected = static_cast<OptionID>(mSelected + 1);
                }
            } else if (input.trigger & VPAD_BUTTON_UP) {
                if (mSelected > OPTION_ID_MIN) {
                    mSelected = static_cast<OptionID>(mSelected - 1);
                }
            }
            break;
        }
        case STATE_ERROR:
            if (input.trigger & VPAD_BUTTON_B) {
                return false;
            }
            break;
    }

    if (mIsApplying) {
        uint8_t newDeviceConfig = mDeviceInfo;

        // Clear/Set the production type mass bit
        if (mIsDevelopment) {
            newDeviceConfig &= ~0x0C;
        } else {
            newDeviceConfig |= 0x04;
        }

        // Clear/Set the shipment flag
        if (mIsUnshipped) {
            newDeviceConfig &= ~0x03;
        } else {
            newDeviceConfig |= 0x01;
        }

        if (!SetDeviceInfo(newDeviceConfig)) {
            mErrorText = "Failed to set device info.\n(Is the firmware modified properly?)";
            mState = STATE_ERROR;
        } else {
            mMessageBox = std::make_unique<MessageBox>(
                "Device info updated!",
                "Restart the Gamepad to apply changes.",
                std::vector{
                    MessageBox::Option{0, "\ue000 OK", [this]() {
                        mSelected = OPTION_ID_MIN;
                        mHasChanged = false;
                    }},
                }
            );
        }

        mIsApplying = false;
    }

    return true;
}
