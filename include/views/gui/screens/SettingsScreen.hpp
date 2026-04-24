#ifndef SETTINGSSCREEN_HPP
#define SETTINGSSCREEN_HPP

#include "views/gui/Screen.hpp"
#include "../components/Button.hpp"
#include "raylib.h"

class SettingsScreen : public Screen {
private:
    Texture2D bgTexture;
    Texture2D musicLabel;
    Texture2D sfxLabel;
    Texture2D ellipseOn;
    Texture2D ellipseOff;

    Button btnBack;

    bool musicEnabled;
    bool sfxEnabled;
    float globalScale;

public:
    SettingsScreen();
    void loadAssets() override;
    void unloadAssets() override;
    void update(float dt) override;
    void draw() override;
};

#endif
