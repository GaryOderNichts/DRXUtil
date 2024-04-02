#include "AboutScreen.hpp"

AboutScreen::AboutScreen()
{
    mCreditList.push_back({"Developers:", "GaryOderNichts"});

    mFontList.push_back({"Main Font:", "Wii U System Font"});
    mFontList.push_back({"Icon Font:", "FontAwesome"});
    mFontList.push_back({"Monospace Font:", "Terminus Font"});

    mLinkList.push_back({"GitHub:", ""});
    mLinkList.push_back({"", {"github.com/GaryOderNichts/DRXUtil", true}});
}

AboutScreen::~AboutScreen()
{
}

void AboutScreen::Draw()
{
    DrawTopBar("AboutScreen");

    int yOff = 128;
    yOff = DrawHeader(32, yOff, 896, 0xf121, "Credits");
    yOff = DrawList(32, yOff, 896, mCreditList);
    yOff = DrawHeader(32, yOff, 896, 0xf031, "Fonts");
    yOff = DrawList(32, yOff, 896, mFontList);

    yOff = 128;
    yOff = DrawHeader(992, yOff, 896, 0xf08e, "Links");
    yOff = DrawList(992, yOff, 896, mLinkList);

    DrawBottomBar(nullptr, "\ue044 Exit", "\ue001 Back");
}

bool AboutScreen::Update(VPADStatus& input)
{
    if (input.trigger & VPAD_BUTTON_B) {
        return false;
    }

    return true;
}
