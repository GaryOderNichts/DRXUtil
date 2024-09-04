#pragma once

#include "Screen.hpp"
#include <map>

class EepromScreen : public Screen
{
public:
    EepromScreen();
    virtual ~EepromScreen();

    void Draw();

    bool Update(VPADStatus& input);

private:
    enum State {
        STATE_DUMP,
        STATE_DONE,
        STATE_ERROR,
    } mState = STATE_DUMP;

    std::string mErrorString;

};
