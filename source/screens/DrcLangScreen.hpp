#pragma once

#include "Screen.hpp"
#include "FlashUtils.hpp"

class DrcLangScreen : public Screen
{
public:
//    DrcLangScreen();
    virtual ~DrcLangScreen();

    void Draw();

    bool Update(VPADStatus& input);

    void OnUpdateCompleted(int32_t result);
private:
    enum State {
        STATE_PREPARE,
        STATE_CONFIRM2,
        STATE_UPDATE,
        STATE_FLASHING,
        STATE_ACTIVATE,
        STATE_DONE,
        STATE_ERROR,
    } mState = STATE_PREPARE;

    struct FileEntry {
        uint16_t icon;
        const char* name;
    };

    std::string mErrorString;
    std::string mFirmwarePath;
    FlashUtils::FirmwareHeader mFirmwareHeader;
    int32_t mFlashingProgress;
    bool mUpdateComplete;
    int32_t mUpdateResult;
};
