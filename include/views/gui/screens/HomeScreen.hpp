#ifndef HOMESCREEN_HPP
#define HOMESCREEN_HPP

#include "views/gui/Screen.hpp"
#include "views/gui/components/Button.hpp"
#include "raylib.h"

class HomeScreen : public Screen {
private:
    Texture2D bgTexture;
    Texture2D titleTexture;
    Button btnNewGame;
    Button btnLoadGame;
    Button btnExit;

    float titleScale;
    float titleX, titleY;

    bool showLoadGameMsg;
    float msgTimer;

public:
    HomeScreen();
    void loadAssets() override;
    void unloadAssets() override;
    void update(float dt) override;
    void draw() override;
};

#endif
