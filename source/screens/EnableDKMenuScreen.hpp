#pragma once

#include "Screen.hpp"
#include <map>
#include <nsysccr/cdc.h>

class EnableDKMenuScreen : public Screen
{
public:
    EnableDKMenuScreen();
    virtual ~EnableDKMenuScreen();

    void Draw();

    bool Update(VPADStatus& input);

private:
    enum State {
        STATE_GET_BOARD_CONFIG,
        STATE_SELECT,
        STATE_CONFIRM,
        STATE_UPDATE,
        STATE_DONE,
        STATE_ERROR,
    } mState = STATE_GET_BOARD_CONFIG;

    bool mConfirm;
    uint8_t mBoardConfig;
    bool mDKMenuEnabled;
    std::string mErrorText;
};
