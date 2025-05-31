#pragma once

#include "Screen.hpp"
#include "MessageBox.hpp"
#include <map>
#include <memory>
#include <nsysccr/cdc.h>

class EditDeviceInfoScreen : public Screen
{
public:
    EditDeviceInfoScreen();
    virtual ~EditDeviceInfoScreen();

    void Draw();

    bool Update(VPADStatus& input);

private:
    enum OptionID {
        OPTION_ID_PRODUCTION_TYPE,
        OPTION_ID_SHIPMENT_STATUS,
        OPTION_ID_APPLY_CHANGES,

        OPTION_ID_MIN = OPTION_ID_PRODUCTION_TYPE,
        OPTION_ID_MAX = OPTION_ID_APPLY_CHANGES,
    };

    size_t mSelected;

    enum State {
        STATE_GET_DEVICE_INFO,
        STATE_SELECT,
        STATE_ERROR,
    } mState = STATE_GET_DEVICE_INFO;

    uint8_t mDeviceInfo;
    bool mIsDevelopment;
    bool mIsUnshipped;
    bool mHasChanged;
    bool mIsApplying;
    std::string mErrorText;

    std::unique_ptr<MessageBox> mMessageBox;
};
