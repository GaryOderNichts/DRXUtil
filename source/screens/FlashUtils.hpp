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
    struct FirmwareHeader {
        uint32_t version;
        uint32_t blockSize;
        uint32_t sequencePerSession;
        uint32_t imageSize;
    };
    static bool CopyFile(const std::string& srcPath, const std::string& dstPath);
    static bool ReadFirmwareHeader(const std::string& path, FlashUtils::FirmwareHeader& header);
    static bool CheckVersionSafety(const uint32_t firmver, const uint32_t langver);
    static bool CaffeineInvalidate();
    static bool WaitForEeprom(uint32_t drcSlot);
    static bool ReattachDRC(CCRCDCDestination dest, CCRCDCDrcStateEnum targetState, BOOL unknown);
    static bool ReattachDRH(CCRCDCDrhStateEnum targetState, BOOL unknown);
    static bool AbortUpdate(CCRCDCDestination dest);
    
};

//extern FlashUtils flashUtils;

