#pragma once

#include "Screen.hpp"
#include <map>
#include <coreinit/im.h>

class PairScreen : public Screen
{
public:
    PairScreen();
    virtual ~PairScreen();

    void Draw();

    bool Update(VPADStatus& input);

    void OnPairingCompleted();

private:
    static void SyncButtonCallback(IOSError error, void* arg);
    static inline bool cancelPairing;
    IOSHandle imHandle;
    IMRequest* imRequest;
    IMRequest* imCancelRequest;
    IMEventMask imEventMask;
    enum State {
        STATE_SELECT_TARGET,
        STATE_GETPIN,
        STATE_WAITING,
        STATE_DONE,
        STATE_ERROR,
    } mState = STATE_SELECT_TARGET;
    struct TargetEntry {
        uint16_t icon;
        const char* name;
    };
    enum TargetID {
        DRC0,
        DRC1,
    } mTarget = DRC0;
    std::map<TargetID, TargetEntry> mTargetEntries;
    std::string mErrorString;
    uint32_t pincodebuffer;
    bool mPairingComplete;
};
