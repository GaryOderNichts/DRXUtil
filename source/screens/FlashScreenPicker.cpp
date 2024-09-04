#include "FlashScreenPicker.hpp"
#include "DrhFlashScreen.hpp"
#include "DrcFlashScreen.hpp"
#include "DrcLangScreen.hpp"
#include "DrcFullFlashScreen.hpp"
#include "Gfx.hpp"

#include <vector>

FlashScreenPicker::FlashScreenPicker()
 : mEntries({
        { MENU_ID_DRHFLASH,       { 0xf085, "Flash DRH firmware" }},
        { MENU_ID_DRCFLASH,      { 0xf1c9, "Flash DRC firmware" }},
        { MENU_ID_DRCLANG,      { 0xf302, "Flash DRC language" }},
        { MENU_ID_DRCFULLFLASH,      { 0xe4c7, "Flash DRC fully" }},
        // { MENU_ID_EXIT,    { 0xf057, "Exit" }},
    })
{

}

FlashScreenPicker::~FlashScreenPicker()
{
}

void FlashScreenPicker::Draw()
{
    if (mSubscreen) {
        mSubscreen->Draw();
        return;
    }

    DrawTopBar("FlashScreenPicker");

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

bool FlashScreenPicker::Update(VPADStatus& input)
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
        case MENU_ID_DRHFLASH:
            mSubscreen = std::make_unique<DrhFlashScreen>();
            break;
        case MENU_ID_DRCFLASH:
            mSubscreen = std::make_unique<DrcFlashScreen>();
            break;
        case MENU_ID_DRCLANG:
            mSubscreen = std::make_unique<DrcLangScreen>();
            break;
        case MENU_ID_DRCFULLFLASH:
            mSubscreen = std::make_unique<DrcFullFlashScreen>();
            break;
        }
    }
    
    if (input.trigger & VPAD_BUTTON_B) {
                return false;
            }
    return true;
}
