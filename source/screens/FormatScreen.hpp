#pragma once

#include "Screen.hpp"
#include <map>

class FormatScreen : public Screen
{
public:
    FormatScreen();
    virtual ~FormatScreen();

    void Draw();

    bool Update(VPADStatus& input);

private:
    enum State {
        STATE_UPDATE,
        STATE_DONE,
        STATE_ERROR,
    } mState = STATE_UPDATE;

    std::string mErrorString;
    

    bool mEraseComplete;
};
