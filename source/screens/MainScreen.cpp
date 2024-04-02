#include "MainScreen.hpp"
#include "MenuScreen.hpp"
#include "Gfx.hpp"

#include <vector>

#include <mocha/mocha.h>
#include <coreinit/filesystem_fsa.h>
#include <nn/ccr.h>

MainScreen::~MainScreen()
{
    if (mState > STATE_INIT_CCR_SYS) {
        CCRSysExit();
    }

    if (mState > STATE_MOUNT_FS) {
        Mocha_UnmountFS("storage_mlc01");
    }

    if (mState > STATE_INIT_MOCHA) {
        Mocha_DeInitLibrary();
    }
}

void MainScreen::Draw()
{
    Gfx::Clear(Gfx::COLOR_BACKGROUND);

    if (mMenuScreen) {
        mMenuScreen->Draw();
        return;
    }

    DrawTopBar(nullptr);

    switch (mState) {
        case STATE_INIT:
            break;
        case STATE_INIT_CCR_SYS:
            DrawStatus("Initializing CCRSys...");
            break;
        case STATE_INIT_MOCHA:
            if (mStateFailure) {
                DrawStatus("Failed to initialize mocha!\nMake sure to update or install Tiramisu/Aroma.", Gfx::COLOR_ERROR);
                break;
            }

            DrawStatus("Initializing mocha...");
            break;
        case STATE_MOUNT_FS:
            if (mStateFailure) {
                DrawStatus("Failed to mount FS!", Gfx::COLOR_ERROR);
                break;
            }

            DrawStatus("Mounting FS...");
            break;
        case STATE_PATCH_IOS:
            if (mStateFailure) {
                DrawStatus("Failed to patch IOS!", Gfx::COLOR_ERROR);
                break;
            }

            DrawStatus("Patching IOS...");
            break;
        case STATE_LOAD_MENU:
            DrawStatus("Loading menu...");
            break;
        case STATE_IN_MENU:
            break;
    }

    DrawBottomBar(mStateFailure ? nullptr : "Please wait...", mStateFailure ? "\ue044 Exit" : nullptr, nullptr);
}

bool MainScreen::Update(VPADStatus& input)
{
    if (mMenuScreen) {
        if (!mMenuScreen->Update(input)) {
            // menu wants to exit
            return false;
        }
        return true;
    }

    MochaUtilsStatus status;
    switch (mState) {
    case STATE_INIT:
        mState = STATE_INIT_CCR_SYS;
        break;
    case STATE_INIT_CCR_SYS:
        CCRSysInit();
        mState = STATE_INIT_MOCHA;
        break;
    case STATE_INIT_MOCHA:
        status = Mocha_InitLibrary();
        if (status != MOCHA_RESULT_SUCCESS) {
            mStateFailure = true;
            break;
        }

        mState = STATE_MOUNT_FS;
        break;
    case STATE_MOUNT_FS:
        // We need access to the MLC to read/write DRC firmware
        status = Mocha_MountFS("storage_mlc01", nullptr, "/vol/storage_mlc01");
        if (status != MOCHA_RESULT_SUCCESS) {
            mStateFailure = true;
            break;
        }
        
        mState = STATE_PATCH_IOS;
        break;
    case STATE_PATCH_IOS:
        uint32_t op;
        if (Mocha_IOSUKernelRead32(0x11f53cf4, &op) != MOCHA_RESULT_SUCCESS) {
            mStateFailure = true;
            break;
        }

        if (op == 0xeb00bd82) {
            // nop IOSU version check
            status = Mocha_IOSUKernelWrite32(0x11f53cf4, 0xe3a00001);
            if (status != MOCHA_RESULT_SUCCESS) {
                mStateFailure = true;
                break;
            }
        } else if (op == 0xe3a00001) {
            // already patched
        } else {
            mStateFailure = true;
            break;
        }

        mState = STATE_LOAD_MENU;
        break;
    case STATE_LOAD_MENU:
        mMenuScreen = std::make_unique<MenuScreen>();
        mState = STATE_IN_MENU;
        break;
    case STATE_IN_MENU:
        break;
    };

    return true;
}

void MainScreen::DrawStatus(std::string status, SDL_Color color)
{
    Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 64, color, status, Gfx::ALIGN_CENTER);
}
