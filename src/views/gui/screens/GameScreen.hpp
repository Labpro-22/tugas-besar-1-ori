#ifndef GAMESCREEN_HPP
#define GAMESCREEN_HPP

#include "../Screen.hpp"
#include "../components/Button.hpp"
#include "raylib.h"
#include <vector>
#include <string>

struct MockPlayer {
    std::string name;
    int money;
    int tileIndex;
    Color color;
};

class GameScreen : public Screen {
private:
    Texture2D bgTexture;
    Texture2D boardTexture;
    Texture2D diceFaces[6];
    Texture2D playerIcons[6];
    Texture2D cardTex;

    Button btnSettings;
    Button btnRollDice;
    Button btnEndTurn;

    std::vector<MockPlayer> players;
    int currentPlayerIdx;
    int die1, die2;
    bool diceRolled;
    int turnCount;
    int maxTurn;

    float boardScale;
    float boardX, boardY;
    float globalScale;

    std::vector<std::string> logMessages;

    void computeBoardLayout();
    void drawHUD();
    void drawPlayerTokens();
    void drawLogPanel();
    Vector2 getTilePosition(int tileIndex);

public:
    GameScreen();
    void loadAssets() override;
    void unloadAssets() override;
    void update(float dt) override;
    void draw() override;
};

#endif
