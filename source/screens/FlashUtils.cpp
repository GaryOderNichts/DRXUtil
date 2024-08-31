#include "FlashUtils.hpp"
#include <cstdio>

#include <coreinit/mcp.h>
#include <coreinit/thread.h>
#include <coreinit/filesystem_fsa.h>
#include <nsysccr/cdc.h>
#include <nsysccr/cfg.h>
#include <nn/ccr.h>

FlashUtils flashUtils;

bool FlashUtils::CopyFile(const std::string& srcPath, const std::string& dstPath)
{
    FILE* inf = fopen(srcPath.c_str(), "rb");
    if (!inf) {
        return false;
    }

    FILE* outf = fopen(dstPath.c_str(), "wb");
    if (!outf) {
        fclose(inf);
        return false;
    }

    uint8_t buf[4096];
    size_t bytesRead;
    while ((bytesRead = fread(buf, 1, sizeof(buf), inf)) > 0) {
        if (fwrite(buf, 1, bytesRead, outf) != bytesRead) {
            fclose(inf);
            fclose(outf);
            return false;
        }
    }

    if (ferror(inf)) {
        fclose(inf);
        fclose(outf);
        return false;
    }

    fclose(inf);
    fclose(outf);
    return true;
}

bool FlashUtils::ReadFirmwareHeader(const std::string& path, FlashUtils::FirmwareHeader& header)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }

    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) {
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

bool FlashUtils::CheckVersionSafety(const uint32_t firmver, const uint32_t langver)
{
    // Very crude way to do the checks universally, but it works well enough
    // We take the firmware version as well as the language version (w/o region - am I stupid?) and compare them 
    if ((firmver == 0x190c0117 || firmver == 0xfe000000) && ((langver & 0xFFFF00) ^ 0x170200)==0){ // Only patched firmware uses language v5890
        return true;
    } else if (firmver == 0x18140116 && not ((langver & 0xFFFF00) ^ 0x160400)){
        return true;
    } else if (firmver == 0x17080114 && not ((langver & 0xFFFF00) ^ 0x140000)){
        return true;
    } else if ((firmver == 0x151e0113 || firmver == 0x15060113 || firmver == 0x14060113) && not ((langver & 0xFFFF00) ^ 0x130000)){ // 3 versions have used language v4864
        return true;
    } else {
        return false;
    }
}

bool FlashUtils::CaffeineInvalidate()
{
    CCRCDCSoftwareVersion version;
    CCRCDCSoftwareGetVersion(CCR_CDC_DESTINATION_DRC0, &version);

    // Only newer versions have caffeine
    if (version.runningVersion >= 0x180a0000) {
        return CCRSysCaffeineSetCaffeineSlot(0xff) == 0;
    }

    return true;
}

bool FlashUtils::WaitForEeprom(uint32_t drcSlot)
{
    uint8_t val;
    OSTime startTime = OSGetSystemTime();
    while (CCRCFGGetCachedEeprom(drcSlot, 0, &val, sizeof(val)) == -1) {
        // 2 second timeout
        if (OSTicksToSeconds(OSGetSystemTime() - startTime) > 2) {
            return false;
        }

        OSSleepTicks(OSMillisecondsToTicks(200));
    }

    return true;
}

bool FlashUtils::ReattachDRC(CCRCDCDestination dest, CCRCDCDrcStateEnum targetState, BOOL unknown)
{
    // Get the current DRC state
    CCRCDCDrcState state;
    int32_t res = CCRCDCSysGetDrcState(dest, &state);
    if (res != 0) {
        return false;
    }

    // Not sure what state 3 is
    if (state.state == CCR_CDC_DRC_STATE_STANDALONE) {
        state.state = CCR_CDC_DRC_STATE_ACTIVE;
    }

    // Nothing to do if we're already in the target state
    if (state.state == targetState) {
        return true;
    }

    __CCRSysInitReattach(dest - CCR_CDC_DESTINATION_DRC0);

    // Set target state
    state.state = targetState;
    res = CCRCDCSysSetDrcState(dest, &state);
    if (res != 0) {
        return false;
    }

    // Wait for the DRC to reattach
    res = __CCRSysWaitReattach(dest - CCR_CDC_DESTINATION_DRC0, unknown);
    if (res != 0) {
        return false;
    }

    // Wait for EEPROM
    if (!WaitForEeprom(dest - CCR_CDC_DESTINATION_DRC0)) {
        return false;
    }

    // Check if we're in the state we want
    res = CCRCDCSysGetDrcState(dest, &state);
    if (res != 0) {
        return false;
    }

    if (state.state != targetState) {
        return false;
    }

    return true;
}

bool FlashUtils::ReattachDRH(CCRCDCDrhStateEnum targetState, BOOL unknown)
{
    // Get the current DRC state
    CCRCDCDrhState state;
    int32_t res = CCRCDCSysGetDrhState(&state);
    if (res != 0) {
        return false;
    }

    // Nothing to do if we're already in the target state
    if (state.state == targetState) {
        return true;
    }

    // Set target state
    state.state = targetState;
    res = CCRCDCSysSetDrhState(&state);
    if (res != 0) {
        return false;
    }

    // Check if we're in the state we want
    res = CCRCDCSysGetDrhState(&state);
    if (res != 0) {
        return false;
    }

    if (state.state != targetState) {
        return false;
    }

    return true;
}


bool FlashUtils::AbortUpdate(CCRCDCDestination dest)
{
    OSTime startTime = OSGetSystemTime();
    while (CCRCDCSoftwareAbort(dest) != 0) {
        // 3 second timeout
        if (OSTicksToSeconds(OSGetSystemTime() - startTime) > 3) {
            return false;
        }

        OSSleepTicks(OSMillisecondsToTicks(200));
    }

    return true;
}
/*
void SoftwareUpdateCallback(IOSError error, void* arg)
{
    FlashUtils* flashUtils = static_cast<FlashUtils*>(arg);

    flashUtils->OnUpdateCompleted(error);
}
*/