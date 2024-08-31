#pragma once

#include "Screen.hpp"
#include "FlashUtils.hpp"
#include <map>

class DrhFlashScreen : public Screen
{

public:
    DrhFlashScreen();
    virtual ~DrhFlashScreen();

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

    enum FileID {
        FILE_ORIGINAL,
        FILE_SDCARD,
    } mFile = FILE_ORIGINAL;
    struct FileEntry {
        uint16_t icon;
        const char* name;
    };
    std::map<FileID, FileEntry> mFileEntries;

    std::string mErrorString;
    std::string mFirmwarePath;
    FlashUtils::FirmwareHeader mFirmwareHeader;

    int32_t mFlashingProgress;
    bool mUpdateComplete;
    int32_t mUpdateResult;
};
