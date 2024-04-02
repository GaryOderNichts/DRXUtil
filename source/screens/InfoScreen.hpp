#pragma once

#include "Screen.hpp"

class InfoScreen : public Screen
{
public:
    InfoScreen();
    virtual ~InfoScreen();

    void Draw();

    bool Update(VPADStatus& input);

private:
    ScreenList mDRCList;
    ScreenList mExtIdList;

    ScreenList mDRHList;
};
