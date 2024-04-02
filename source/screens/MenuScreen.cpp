#include "MenuScreen.hpp"
#include "Gfx.hpp"
#include "AboutScreen.hpp"
#include "FlashScreen.hpp"
#include "InfoScreen.hpp"
#include "SetRegionScreen.hpp"

#include <vector>

MenuScreen::MenuScreen()
 : mEntries({
        { MENU_ID_INFO,       { 0xf085, "Show DRC/DRH information" }},
        { MENU_ID_FLASH,      { 0xf1c9, "Flash firmware" }},
        { MENU_ID_SET_REGION, { 0xf0ac, "Set region" }},
        { MENU_ID_ABOUT,      { 0xf05a, "About DRXUtil" }},
        // { MENU_ID_EXIT,    { 0xf057, "Exit" }},
    })
{

}

MenuScreen::~MenuScreen()
{
}

void MenuScreen::Draw()
{
    if (mSubscreen) {
        mSubscreen->Draw();
        return;
    }

    DrawTopBar(nullptr);

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

    DrawBottomBar("\ue07d Navigate", "\ue044 Exit", "\ue000 Select");
}

bool MenuScreen::Update(VPADStatus& input)
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
        case MENU_ID_INFO:
            mSubscreen = std::make_unique<InfoScreen>();
            break;
        case MENU_ID_FLASH:
            mSubscreen = std::make_unique<FlashScreen>();
            break;
        case MENU_ID_SET_REGION:
            mSubscreen = std::make_unique<SetRegionScreen>();
            break;
        case MENU_ID_ABOUT:
            mSubscreen = std::make_unique<AboutScreen>();
            break;
        }
    }

    return true;
}
