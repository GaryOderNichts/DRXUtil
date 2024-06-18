#pragma once

#include "Screen.hpp"
#include <memory>
#include <map>

class MenuScreen : public Screen
{
public:
    MenuScreen();
    virtual ~MenuScreen();

    void Draw();

    bool Update(VPADStatus& input);

private:
    std::unique_ptr<Screen> mSubscreen;

    enum MenuID {
        MENU_ID_INFO,
        MENU_ID_FLASH,
        MENU_ID_SET_REGION,
        MENU_ID_ENABLE_DKMENU,
        MENU_ID_ABOUT,

        MENU_ID_MIN = MENU_ID_INFO,
        MENU_ID_MAX = MENU_ID_ABOUT,
    };

    struct MenuEntry {
        uint16_t icon;
        const char* name;
    };
    std::map<MenuID, MenuEntry> mEntries;
    MenuID mSelected = MENU_ID_MIN;
};
