#pragma once

#include "Screen.hpp"
#include "Gfx.hpp"
#include <memory>

class MainScreen : public Screen
{
public:
    MainScreen() = default;
    virtual ~MainScreen();

    void Draw();

    bool Update(VPADStatus& input);

protected:
    void DrawStatus(std::string status, SDL_Color color = Gfx::COLOR_TEXT);

private:
    enum {
        STATE_INIT,
        STATE_INIT_CCR_SYS,
        STATE_INIT_MOCHA,
        STATE_MOUNT_FS,
        STATE_PATCH_IOS,
        STATE_LOAD_MENU,
        STATE_IN_MENU,
    } mState = STATE_INIT;
    bool mStateFailure = false;

    std::unique_ptr<Screen> mMenuScreen;
};
