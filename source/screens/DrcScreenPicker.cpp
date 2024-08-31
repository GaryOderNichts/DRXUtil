#include "DrcScreenPicker.hpp"
#include "SetRegionScreen.hpp"
#include "EepromScreen.hpp"
#include "PairScreen.hpp"
#include "EnableDKMenuScreen.hpp"
#include "FormatScreen.hpp"
#include "Gfx.hpp"

#include <vector>

DrcScreenPicker::DrcScreenPicker()
 : mEntries({
        { MENU_ID_SET_REGION,       { 0xf0ac, "Set region" }},
        { MENU_ID_DUMP_EEPROM,      { 0xf08b, "Dump EEPROMs" }},
        { MENU_ID_DRC_PAIR,         { 0xf0c1, "Pair DRC..." }},
        { MENU_ID_ENABLE_DKMENU,    { 0xf188, "Enable DK Menu" }},
        { MENU_ID_DRC_RESET,        { 0xf079, "Reset DRC" }},
    })
{

}

DrcScreenPicker::~DrcScreenPicker()
{
}

void DrcScreenPicker::Draw()
{
    if (mSubscreen) {
        mSubscreen->Draw();
        return;
    }

    DrawTopBar("DrcScreenPicker");

    // draw entries
    for (MenuID id = MENU_ID_MIN; id <= MENU_ID_MAX; id = static_cast<MenuID>(id + 1)) {
        int yOff = 75 + static_cast<int>(id) * 150;
        Gfx::DrawRectFilled(0, yOff, Gfx::SCREEN_WIDTH, 150, Gfx::COLOR_ALT_BACKGROUND);
        Gfx::DrawIcon(68, yOff + 150 / 2, 60, Gfx::COLOR_TEXT, mEntries[id].icon);
        Gfx::Print(128 + 8, yOff + 150 / 2, 60, Gfx::COLOR_TEXT, mEntries[id].name, Gfx::ALIGN_VERTICAL);

        if (id == mSelected) {
            Gfx::DrawRect(0, yOff, Gfx::SCREEN_WIDTH, 150, 8, Gfx::COLOR_HIGHLIGHTED);
        }
    }

    DrawBottomBar("\ue07d Navigate", "\ue044 Exit", "\ue000 Select / \ue001 Back");
}

bool DrcScreenPicker::Update(VPADStatus& input)
{
    if (mSubscreen) {
        if (!mSubscreen->Update(input)) {
            // subscreen wants to exit
            mSubscreen.reset();
        }
        return true;
    }

    if (input.trigger & VPAD_BUTTON_DOWN) {
        if (mSelected < MENU_ID_MAX) {
            mSelected = static_cast<MenuID>(mSelected + 1);
        }
    } else if (input.trigger & VPAD_BUTTON_UP) {
        if (mSelected > MENU_ID_MIN) {
            mSelected = static_cast<MenuID>(mSelected - 1);
        }
    }

    if (input.trigger & VPAD_BUTTON_A) {
        switch (mSelected) {
        case MENU_ID_SET_REGION:
            mSubscreen = std::make_unique<SetRegionScreen>();
            break;
        case MENU_ID_DUMP_EEPROM:
            mSubscreen = std::make_unique<EepromScreen>();
            break;
        case MENU_ID_DRC_PAIR:
            mSubscreen = std::make_unique<PairScreen>();
            break;
        case MENU_ID_ENABLE_DKMENU:
            mSubscreen = std::make_unique<EnableDKMenuScreen>();
            break;
        case MENU_ID_DRC_RESET:
            mSubscreen = std::make_unique<FormatScreen>();
            break;
        }
    }
    
    if (input.trigger & VPAD_BUTTON_B) {
                return false;
            }
    return true;
}
