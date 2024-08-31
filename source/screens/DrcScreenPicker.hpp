#pragma once

#include "Screen.hpp"
#include <memory>
#include <map>

class DrcScreenPicker : public Screen
{
public:
    DrcScreenPicker();
    virtual ~DrcScreenPicker();

    void Draw();

    bool Update(VPADStatus& input);

private:
    std::unique_ptr<Screen> mSubscreen;

    enum MenuID {
        MENU_ID_SET_REGION,
        MENU_ID_DUMP_EEPROM,
        MENU_ID_DRC_PAIR,
        MENU_ID_ENABLE_DKMENU,
        MENU_ID_DRC_RESET,

        MENU_ID_MIN = MENU_ID_SET_REGION,
        MENU_ID_MAX = MENU_ID_DRC_RESET,
    };

    struct MenuEntry {
        uint16_t icon;
        const char* name;
    };
    std::map<MenuID, MenuEntry> mEntries;
    MenuID mSelected = MENU_ID_MIN;
};
