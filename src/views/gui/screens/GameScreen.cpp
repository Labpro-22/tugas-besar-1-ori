#include "GameScreen.hpp"
#include <cmath>
#include <cstdlib>

GameScreen::GameScreen()
    : bgTexture{}, boardTexture{}, cardTex{},
      currentPlayerIdx(0), die1(1), die2(1), diceRolled(false),
      turnCount(1), maxTurn(15), boardScale(1.0f), boardX(0), boardY(0), globalScale(1.0f) {
    for (int i = 0; i < 6; i++) {
        diceFaces[i] = {};
        playerIcons[i] = {};
    }
    players = {
        {"nus", 1500, 0, RED},
        {"jo", 1500, 0, BLUE},
        {"bot", 1400, 0, GREEN}
    };
    logMessages.push_back("[Turn 1] Game started!");
    logMessages.push_back("[Turn 1] nus rolled 4+3=7");
    logMessages.push_back("[Turn 1] nus landed on Bandung");
}

void GameScreen::loadAssets() {
    bgTexture = LoadTexture("assets/page_background.png");
    boardTexture = LoadTexture("assets/board.png");
    cardTex = LoadTexture("assets/card1.png");

    diceFaces[0] = LoadTexture("assets/assets1/dice-1-export 1.png");
    diceFaces[1] = LoadTexture("assets/assets1/dice-2-export 1.png");
    diceFaces[2] = LoadTexture("assets/assets1/dice-3-export 1.png");
    diceFaces[3] = LoadTexture("assets/assets1/dice-4-export 1.png");
    diceFaces[4] = LoadTexture("assets/assets1/dice-5-export 1.png");
    diceFaces[5] = LoadTexture("assets/assets1/dice-6-export 1.png");

    playerIcons[0] = LoadTexture("assets/assets1/icon-1.png");
    playerIcons[1] = LoadTexture("assets/assets1/icon-2.png");
    playerIcons[2] = LoadTexture("assets/assets1/icon-3.png");
    playerIcons[3] = LoadTexture("assets/assets1/icon-4.png");
    playerIcons[4] = LoadTexture("assets/assets1/icon-5.png");
    playerIcons[5] = LoadTexture("assets/assets1/icon-6.png");

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    globalScale = std::min(sw / 1280.0f, sh / 720.0f);

    computeBoardLayout();

    float btnScale = globalScale * 0.25f;
    btnSettings.load("assets/settings/dice-6-export 2.png", sw - 80 * globalScale, 10 * globalScale, btnScale);
    btnRollDice.load("assets/settings/dice-6-export 3.png", sw * 0.72f, sh * 0.55f, btnScale * 1.5f);
    btnEndTurn.load("assets/new-game-page/start 2.png", sw * 0.72f, sh * 0.72f, btnScale);
}

void GameScreen::unloadAssets() {
    UnloadTexture(bgTexture);
    UnloadTexture(boardTexture);
    for (int i = 0; i < 6; i++) {
        UnloadTexture(diceFaces[i]);
        UnloadTexture(playerIcons[i]);
    }
    UnloadTexture(cardTex);
    btnSettings.unload();
    btnRollDice.unload();
    btnEndTurn.unload();
}

void GameScreen::computeBoardLayout() {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    float maxBoardH = sh * 0.88f;
    float maxBoardW = sw * 0.60f;
    boardScale = std::min(maxBoardW / boardTexture.width, maxBoardH / boardTexture.height);
    boardX = 20 * globalScale;
    boardY = (sh - boardTexture.height * boardScale) / 2.0f;
}

Vector2 GameScreen::getTilePosition(int tileIndex) {
    int totalTiles = 40;
    float w = boardTexture.width * boardScale;
    float h = boardTexture.height * boardScale;
    float side = w;
    float perSide = totalTiles / 4.0f;

    float normX = boardX;
    float normY = boardY;

    if (tileIndex >= 0 && tileIndex <= 10) {
        float t = tileIndex / perSide;
        return {normX + side * (1.0f - t), normY + h};
    } else if (tileIndex > 10 && tileIndex <= 20) {
        float t = (tileIndex - 10) / perSide;
        return {normX, normY + h * (1.0f - t)};
    } else if (tileIndex > 20 && tileIndex <= 30) {
        float t = (tileIndex - 20) / perSide;
        return {normX + side * t, normY};
    } else {
        float t = (tileIndex - 30) / perSide;
        return {normX + side, normY + h * t};
    }
}

void GameScreen::update(float dt) {
    (void)dt;

    if (btnSettings.isClicked()) {
        nextScreen = AppScreen::SETTINGS;
        shouldChangeScreen = true;
        return;
    }

    if (btnRollDice.isClicked()) {
        die1 = 1 + rand() % 6;
        die2 = 1 + rand() % 6;
        diceRolled = true;
        int total = die1 + die2;
        MockPlayer& p = players[currentPlayerIdx];
        p.tileIndex = (p.tileIndex + total) % 40;
        logMessages.push_back(TextFormat("[Turn %d] %s rolled %d+%d=%d", turnCount,
                                          p.name.c_str(), die1, die2, total));
        logMessages.push_back(TextFormat("[Turn %d] %s landed on tile %d", turnCount,
                                          p.name.c_str(), p.tileIndex));
        if (logMessages.size() > 20) {
            logMessages.erase(logMessages.begin());
        }
    } else if (btnEndTurn.isClicked()) {
        diceRolled = false;
        currentPlayerIdx = (currentPlayerIdx + 1) % players.size();
        if (currentPlayerIdx == 0) turnCount++;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        nextScreen = AppScreen::HOME;
        shouldChangeScreen = true;
        return;
    }

    if (IsWindowResized()) {
        unloadAssets();
        loadAssets();
    }
}

void GameScreen::draw() {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    float bgScale = std::max(sw / (float)bgTexture.width, sh / (float)bgTexture.height);
    float bgX = (sw - bgTexture.width * bgScale) / 2.0f;
    float bgY = (sh - bgTexture.height * bgScale) / 2.0f;
    DrawTextureEx(bgTexture, {bgX, bgY}, 0.0f, bgScale, WHITE);

    DrawTextureEx(boardTexture, {boardX, boardY}, 0.0f, boardScale, WHITE);

    drawPlayerTokens();
    drawHUD();
    drawLogPanel();

    btnSettings.draw();
    btnRollDice.draw();
    btnEndTurn.draw();
}

void GameScreen::drawPlayerTokens() {
    for (size_t i = 0; i < players.size(); i++) {
        Vector2 pos = getTilePosition(players[i].tileIndex);
        float offsetX = i * 18 * globalScale;
        float offsetY = -10 * globalScale;
        float iconScale = globalScale * 0.08f;
        Texture2D icon = playerIcons[i];
        float iw = icon.width * iconScale;
        float ih = icon.height * iconScale;
        float drawX = pos.x + offsetX - iw / 2;
        float drawY = pos.y + offsetY - ih;
        DrawTextureEx(icon, {drawX, drawY}, 0.0f, iconScale, WHITE);
        if (i == static_cast<size_t>(currentPlayerIdx)) {
            DrawRectangleLines(static_cast<int>(drawX - 2), static_cast<int>(drawY - 2),
                               static_cast<int>(iw + 4), static_cast<int>(ih + 4), GOLD);
        }
    }
}

void GameScreen::drawHUD() {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    float panelX = sw * 0.68f;
    float panelY = sh * 0.08f;
    float panelW = sw * 0.30f;
    float panelH = sh * 0.35f;

    DrawRectangle(static_cast<int>(panelX), static_cast<int>(panelY),
                  static_cast<int>(panelW), static_cast<int>(panelH),
                  Fade(BLACK, 0.6f));
    DrawRectangleLines(static_cast<int>(panelX), static_cast<int>(panelY),
                       static_cast<int>(panelW), static_cast<int>(panelH),
                       GOLD);

    int fontSize = static_cast<int>(18 * globalScale);
    int lineH = static_cast<int>(24 * globalScale);
    int textX = static_cast<int>(panelX + 10 * globalScale);
    int textY = static_cast<int>(panelY + 10 * globalScale);

    DrawText("=== PLAYER STATUS ===", textX, textY, fontSize, GOLD);
    textY += lineH + 5;

    for (size_t i = 0; i < players.size(); i++) {
        const MockPlayer& p = players[i];
        Color c = (i == static_cast<size_t>(currentPlayerIdx)) ? YELLOW : WHITE;
        float iconScale = globalScale * 0.035f;
        Texture2D icon = playerIcons[i];
        DrawTextureEx(icon, {static_cast<float>(textX), static_cast<float>(textY)}, 0.0f, iconScale, WHITE);
        DrawText(TextFormat(" %s%s  M%d  Pos:%d",
                            (i == static_cast<size_t>(currentPlayerIdx)) ? "> " : "  ",
                            p.name.c_str(), p.money, p.tileIndex),
                 static_cast<int>(textX + icon.width * iconScale + 4), textY, fontSize, c);
        textY += lineH;
    }

    textY += lineH / 2;
    DrawText(TextFormat("Turn: %d / %d", turnCount, maxTurn), textX, textY, fontSize, WHITE);

    if (diceRolled) {
        float diceScale = globalScale * 0.25f;
        Texture2D d1 = diceFaces[die1 - 1];
        Texture2D d2 = diceFaces[die2 - 1];
        float d1w = d1.width * diceScale;
        float diceY = panelY + panelH * 0.72f;
        DrawTextureEx(d1, {panelX + panelW * 0.15f, diceY}, 0.0f, diceScale, WHITE);
        DrawTextureEx(d2, {panelX + panelW * 0.55f, diceY}, 0.0f, diceScale, WHITE);
        int diceFont = static_cast<int>(20 * globalScale);
        DrawText(TextFormat("%d  %d", die1, die2),
                 static_cast<int>(panelX + panelW * 0.35f),
                 static_cast<int>(diceY + d1w + 5),
                 diceFont, WHITE);
    } else {
        DrawText("Click Roll Dice", static_cast<int>(panelX + panelW * 0.25f),
                 static_cast<int>(panelY + panelH * 0.85f),
                 static_cast<int>(16 * globalScale), GRAY);
    }
}

void GameScreen::drawLogPanel() {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    float panelX = sw * 0.68f;
    float panelY = sh * 0.48f;
    float panelW = sw * 0.30f;
    float panelH = sh * 0.42f;

    DrawRectangle(static_cast<int>(panelX), static_cast<int>(panelY),
                  static_cast<int>(panelW), static_cast<int>(panelH),
                  Fade(BLACK, 0.6f));
    DrawRectangleLines(static_cast<int>(panelX), static_cast<int>(panelY),
                       static_cast<int>(panelW), static_cast<int>(panelH),
                       DARKGRAY);

    int fontSize = static_cast<int>(14 * globalScale);
    int lineH = static_cast<int>(18 * globalScale);
    int textX = static_cast<int>(panelX + 8 * globalScale);
    int textY = static_cast<int>(panelY + 8 * globalScale);

    DrawText("--- LOG ---", textX, textY, fontSize, GRAY);
    textY += lineH + 2;

    int maxLines = static_cast<int>((panelH - 30 * globalScale) / lineH);
    int startIdx = static_cast<int>(logMessages.size()) - maxLines;
    if (startIdx < 0) startIdx = 0;

    for (size_t i = startIdx; i < logMessages.size(); i++) {
        DrawText(logMessages[i].c_str(), textX, textY, fontSize, LIGHTGRAY);
        textY += lineH;
    }
}
