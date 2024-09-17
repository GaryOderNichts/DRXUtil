#pragma once

#include "Screen.hpp"
#include <map>
#include <nsysccr/cdc.h>

class SetRegionScreen : public Screen
{
public:
    SetRegionScreen();
    virtual ~SetRegionScreen();

    void Draw();

    bool Update(VPADStatus& input);

private:
    enum State {
        STATE_SELECT_REGION,
        STATE_CONFIRM,
        STATE_UPDATE,
        STATE_DONE,
        STATE_ERROR,
    } mState = STATE_SELECT_REGION;

    // From gamepad firmware @0x000b29fc
    enum Region {
        REGION_JAPAN         = 0,
        REGION_AMERICA       = 1,
        REGION_EUROPE        = 2,
        REGION_CHINA         = 3,
        REGION_SOUTH_KOREA   = 4,
        REGION_TAIWAN        = 5,
        REGION_AUSTRALIA     = 6,
    } mRegion = REGION_JAPAN;
    std::map<Region, std::string> mRegionEntries;
};
