#pragma once

#include "Screen.hpp"

#include <coreinit/mcp.h>
#include <coreinit/thread.h>
#include <coreinit/filesystem_fsa.h>
#include <nsysccr/cdc.h>
#include <nsysccr/cfg.h>
#include <nn/ccr.h>

class FlashUtils
{
public:
    void OnUpdateCompleted(int32_t result);
    struct FirmwareHeader {
        uint32_t version;
        uint32_t blockSize;
        uint32_t sequencePerSession;
        uint32_t imageSize;
    };
    bool CopyFile(const std::string& srcPath, const std::string& dstPath);
    bool ReadFirmwareHeader(const std::string& path, FlashUtils::FirmwareHeader& header);
    bool CheckVersionSafety(const uint32_t firmver, const uint32_t langver);
    bool CaffeineInvalidate();
    bool WaitForEeprom(uint32_t drcSlot);
    bool ReattachDRC(CCRCDCDestination dest, CCRCDCDrcStateEnum targetState, BOOL unknown);
    bool ReattachDRH(CCRCDCDrhStateEnum targetState, BOOL unknown);
    bool AbortUpdate(CCRCDCDestination dest);
    
};

extern FlashUtils flashUtils;

