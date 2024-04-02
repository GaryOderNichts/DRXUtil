#pragma once

#include "Screen.hpp"

class AboutScreen : public Screen
{
public:
    AboutScreen();
    virtual ~AboutScreen();

    void Draw();

    bool Update(VPADStatus& input);

private:
    ScreenList mCreditList;
    ScreenList mFontList;
    ScreenList mLinkList;
};
