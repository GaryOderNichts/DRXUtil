#pragma once

#include "Screen.hpp"
#include <memory>
#include <map>

class FlashScreenPicker : public Screen
{
public:
    FlashScreenPicker();
    virtual ~FlashScreenPicker();

    void Draw();

    bool Update(VPADStatus& input);

private:
    std::unique_ptr<Screen> mSubscreen;

    enum MenuID {
        MENU_ID_DRHFLASH,
        MENU_ID_DRCFLASH,
        MENU_ID_DRCLANG,
        MENU_ID_DRCFFLASH,

        MENU_ID_MIN = MENU_ID_DRHFLASH,
        MENU_ID_MAX = MENU_ID_DRCFFLASH,
    };

    struct MenuEntry {
        uint16_t icon;
        const char* name;
    };
    std::map<MenuID, MenuEntry> mEntries;
    MenuID mSelected = MENU_ID_MIN;
};
