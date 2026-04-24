#ifndef GAMESCREEN_HPP
#define GAMESCREEN_HPP

#include "../Screen.hpp"
#include "../components/Button.hpp"
#include "config/GameConfig.hpp"
#include "raylib.h"

class GameScreen : public Screen {
private:
    Texture2D bgTexture;
    Texture2D blurredBgTexture;
    Texture2D boardTexture;
    Texture2D playerIcons[6];
    Texture2D cardPanel;
    Texture2D diceTextures[6];
    Texture2D houseTexture;
    Texture2D hotelTexture;
    Texture2D pamTexture;
    Texture2D plnTexture;
    Texture2D trainTexture;

    Button btnPlay;
    Button btnAssets;
    Button btnPlayers;
    Button btnLog;
    Button btnRollDice;
    Button btnEndTurn;

    Button btnBuyProperty;
    Button btnAuction;
    Button btnBuildHouse;
    Button btnMortgage;

    int activeTab;
    int currentPlayerIdx;
    int numPlayers;

    int dice1;
    int dice2;
    bool diceRolling;
    float diceRollTimer;
    float diceRollInterval;
    int diceRollFrame;

    bool hasRolled;
    bool turnEnded;

    float globalScale;
    float boardX, boardY, boardScale;
    float cornerRatio;
    float tileCenters[40][2];

    GameConfig* gameConfig;

    void computeLayout();

public:
    GameScreen(GameConfig& config);
    void loadAssets() override;
    void unloadAssets() override;
    void update(float dt) override;
    void draw() override;
};

#endif