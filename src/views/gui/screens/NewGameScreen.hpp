#ifndef NEWGAMESCREEN_HPP
#define NEWGAMESCREEN_HPP

#include "../Screen.hpp"
#include "../components/Button.hpp"
#include "config/GameConfig.hpp"
#include "raylib.h"
#include <vector>

class NewGameScreen : public Screen {
private:
    Texture2D bgTexture;
    Texture2D playerCountLabel;
    Texture2D selectIconsLabel;
    Texture2D minMaxLabel;
    Texture2D counterTex;
    Texture2D playerIcons[6];

    Button btnBack;
    Button btnStart;
    Button btnPlus;
    Button btnMinus;

    int playerCount;
    bool iconSelected[6];

    float globalScale;

    GameConfig* gameConfig;
    int activeNameField;
    int nameCursorPos[GameConfig::MAX_PLAYERS];

    Rectangle nameFields[GameConfig::MAX_PLAYERS];

public:
    NewGameScreen(GameConfig& config);
    void loadAssets() override;
    void unloadAssets() override;
    void update(float dt) override;
    void draw() override;
};

#endif
