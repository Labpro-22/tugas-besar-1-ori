#include "GameScreen.hpp"
#include <algorithm>

static const float CORNER_RATIO = 0.135f;

static void getGridRC(int i, int& row, int& col) {
    if (i == 0)        { row = 10; col = 10; }
    else if (i <= 9)   { row = 10; col = 10 - i; }
    else if (i == 10)  { row = 10; col = 0; }
    else if (i <= 19)  { row = 20 - i; col = 0; }
    else if (i == 20)  { row = 0; col = 0; }
    else if (i <= 29)  { row = 0; col = i - 20; }
    else if (i == 30)  { row = 0; col = 10; }
    else               { row = i - 30; col = 10; }
}

GameScreen::GameScreen(GameConfig& config)
    : bgTexture{}, blurredBgTexture{}, boardTexture{}, cardPanel{},
      activeTab(0), currentPlayerIdx(0), numPlayers(config.playerCount),
      dice1(1), dice2(1), diceRolling(false), diceRollTimer(0), diceRollInterval(0.08f), diceRollFrame(0),
      hasRolled(false), turnEnded(false),
      globalScale(1.0f), boardX(0), boardY(0), boardScale(1.0f),
      cornerRatio(CORNER_RATIO),
      gameConfig(&config) {
    for (int i = 0; i < 6; i++) playerIcons[i] = {};
    for (int i = 0; i < 6; i++) diceTextures[i] = {};
    for (int i = 0; i < 40; i++) { tileCenters[i][0] = 0; tileCenters[i][1] = 0; }
}

void GameScreen::computeLayout() {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    globalScale = std::min(sw / 1280.0f, sh / 720.0f);

    float maxH = sh * 0.96f;
    float maxW = sw * 0.55f;
    boardScale = std::min(maxW / (float)boardTexture.width, maxH / (float)boardTexture.height);
    boardX = sw * 0.02f;
    boardY = (sh - boardTexture.height * boardScale) / 2.0f;

    float bw = boardTexture.width * boardScale;
    float bh = boardTexture.height * boardScale;
    float cornerSz = bw * cornerRatio;
    float sideSz = (bw - 2.0f * cornerSz) / 9.0f;
    float cornerSzh = bh * cornerRatio;
    float sideSzh = (bh - 2.0f * cornerSzh) / 9.0f;

    for (int i = 0; i < 40; i++) {
        int row, col;
        getGridRC(i, row, col);

        float cx, cy;
        if (col == 0)      cx = boardX + cornerSz / 2.0f;
        else if (col == 10) cx = boardX + bw - cornerSz / 2.0f;
        else                cx = boardX + cornerSz + (col - 1) * sideSz + sideSz / 2.0f;

        if (row == 0)        cy = boardY + cornerSzh / 2.0f;
        else if (row == 10)  cy = boardY + bh - cornerSzh / 2.0f;
        else                 cy = boardY + cornerSzh + (row - 1) * sideSzh + sideSzh / 2.0f;

        tileCenters[i][0] = cx;
        tileCenters[i][1] = cy;
    }

    float boardRight = boardX + boardTexture.width * boardScale;
    float availW = sw - boardRight - 10.0f;
    float cardScale = std::min(availW / (float)cardPanel.width, (sh - 40.0f) / (float)cardPanel.height);
    if (cardScale < 0.1f) cardScale = 0.1f;
    float cardX = boardRight + (availW - cardPanel.width * cardScale) * 0.65f;
    float cardY = (sh - cardPanel.height * cardScale) / 2.0f;

    float panelCenterX = cardX + cardPanel.width * cardScale / 2.0f;
    float btnW = cardPanel.width * cardScale * 0.22f;
    float btnH = 35.0f * globalScale;
    float btnSpacing = 4.0f * globalScale;
    float totalBtnW = 4.0f * btnW + 3.0f * btnSpacing;
    float btnStartX = panelCenterX - totalBtnW / 1.94f;
    float btnY = cardY + cardPanel.height * cardScale * 0.265f;

    btnPlay.loadAsText("PLAY", btnStartX, btnY, btnW, btnH);
    btnAssets.loadAsText("ASSETS", btnStartX + btnW + btnSpacing, btnY, btnW, btnH);
    btnPlayers.loadAsText("PLAYERS", btnStartX + 2.0f * (btnW + btnSpacing), btnY, btnW, btnH);
    btnLog.loadAsText("LOG", btnStartX + 3.0f * (btnW + btnSpacing), btnY, btnW, btnH);
    btnPlay.setActive(activeTab == 0);
    btnAssets.setActive(activeTab == 1);
    btnPlayers.setActive(activeTab == 2);
    btnLog.setActive(activeTab == 3);

    float iconSz = cardPanel.width * cardScale * 0.15f;
    float iconYPos = btnPlay.getY() + btnPlay.getHeight() - 198.0f * globalScale;
    float playerNameBottom = iconYPos + iconSz + 10.0f * globalScale;

    float diceSz = cardPanel.width * cardScale * 0.10f;
    float diceY = playerNameBottom + 18.0f * globalScale;

    float actionBtnW = cardPanel.width * cardScale * 0.2f;
    float actionBtnH = diceSz * 0.85f;
    float rowGap = 10.0f * globalScale;

    float totalRowW = diceSz + diceSz + rowGap + actionBtnW + rowGap + actionBtnW;
    float rowStartX = cardX + (cardPanel.width * cardScale - totalRowW) / 2.0f;
    float actionBtnY = diceY + (diceSz - actionBtnH) / 2.0f;

    float dice1X = rowStartX;
    float dice2X = dice1X + diceSz + rowGap;
    float rollBtnX = dice2X + diceSz + rowGap;
    float endTurnBtnX = rollBtnX + actionBtnW + rowGap;

    btnRollDice.loadAsText("ROLL", rollBtnX, actionBtnY, actionBtnW, actionBtnH);
    btnEndTurn.loadAsText("END TURN", endTurnBtnX, actionBtnY, actionBtnW, actionBtnH);
}

void GameScreen::loadAssets() {
    bgTexture = LoadTexture("assets/page_background.png");
    Image bgImg = LoadImage("assets/page_background.png");
    ImageBlurGaussian(&bgImg, 10);
    blurredBgTexture = LoadTextureFromImage(bgImg);
    UnloadImage(bgImg);

    boardTexture = LoadTexture("assets/board.png");
    cardPanel = LoadTexture("assets/menu/menu_back.png");

    playerIcons[0] = LoadTexture("assets/assets1/icon-1.png");
    playerIcons[1] = LoadTexture("assets/assets1/icon-2.png");
    playerIcons[2] = LoadTexture("assets/assets1/icon-3.png");
    playerIcons[3] = LoadTexture("assets/assets1/icon-4.png");
    playerIcons[4] = LoadTexture("assets/assets1/icon-5.png");
    playerIcons[5] = LoadTexture("assets/assets1/icon-6.png");

    diceTextures[0] = LoadTexture("assets/assets1/dice-1-export 1.png");
    diceTextures[1] = LoadTexture("assets/assets1/dice-2-export 1.png");
    diceTextures[2] = LoadTexture("assets/assets1/dice-3-export 1.png");
    diceTextures[3] = LoadTexture("assets/assets1/dice-4-export 1.png");
    diceTextures[4] = LoadTexture("assets/assets1/dice-5-export 1.png");
    diceTextures[5] = LoadTexture("assets/assets1/dice-6-export 1.png");

    computeLayout();
}

void GameScreen::unloadAssets() {
    UnloadTexture(bgTexture);
    UnloadTexture(blurredBgTexture);
    UnloadTexture(boardTexture);
    UnloadTexture(cardPanel);
    for (int i = 0; i < 6; i++) UnloadTexture(playerIcons[i]);
    for (int i = 0; i < 6; i++) UnloadTexture(diceTextures[i]);
    btnPlay.unload();
    btnAssets.unload();
    btnPlayers.unload();
    btnLog.unload();
    btnRollDice.unload();
    btnEndTurn.unload();
}

void GameScreen::update(float dt) {
    (void)dt;
    if (btnPlay.isClicked()) activeTab = 0;
    if (btnAssets.isClicked()) activeTab = 1;
    if (btnPlayers.isClicked()) activeTab = 2;
    if (btnLog.isClicked()) activeTab = 3;

    btnPlay.setActive(activeTab == 0);
    btnAssets.setActive(activeTab == 1);
    btnPlayers.setActive(activeTab == 2);
    btnLog.setActive(activeTab == 3);

    if (IsKeyPressed(KEY_ESCAPE)) {
        nextScreen = AppScreen::HOME;
        shouldChangeScreen = true;
    }

    if (diceRolling) {
        diceRollTimer += dt;
        if (diceRollTimer >= diceRollInterval) {
            diceRollTimer = 0;
            diceRollFrame++;
            dice1 = GetRandomValue(1, 6);
            dice2 = GetRandomValue(1, 6);
            if (diceRollFrame >= 12) {
                diceRolling = false;
                diceRollFrame = 0;
                hasRolled = true;
            }
        }
    }

    if (btnRollDice.isClicked() && !diceRolling && !hasRolled) {
        diceRolling = true;
        diceRollTimer = 0;
        diceRollFrame = 0;
        diceRollInterval = 0.06f;
    }

    if (btnEndTurn.isClicked() && hasRolled && !diceRolling) {
        turnEnded = true;
        hasRolled = false;
        currentPlayerIdx = (currentPlayerIdx + 1) % numPlayers;
        turnEnded = false;
    }

    if (IsWindowResized()) computeLayout();
}

void GameScreen::draw() {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    float bgScale = std::max(sw / (float)blurredBgTexture.width, sh / (float)blurredBgTexture.height);
    float bgX = (sw - blurredBgTexture.width * bgScale) / 2.0f;
    float bgY = (sh - blurredBgTexture.height * bgScale) / 2.0f;
    DrawTextureEx(blurredBgTexture, {bgX, bgY}, 0.0f, bgScale, WHITE);

    DrawTextureEx(boardTexture, {boardX, boardY}, 0.0f, boardScale, WHITE);

    float iconScale = globalScale * 0.06f;
    int boardNameSize = static_cast<int>(8 * globalScale);
    for (int i = 0; i < numPlayers; i++) {
        float cx = tileCenters[i][0];
        float cy = tileCenters[i][1];
        float iw = playerIcons[i].width * iconScale;
        float ih = playerIcons[i].height * iconScale;
        float dx = cx - iw / 2.0f + (i % 3 - 1) * iw * 0.5f;
        float dy = cy - ih / 2.0f + (i / 3 - 0.5f) * ih * 0.4f;
        DrawTextureEx(playerIcons[i], {dx, dy}, 0.0f, iconScale, WHITE);

        int nameW = MeasureText(gameConfig->playerNames[i], boardNameSize);
        DrawText(gameConfig->playerNames[i],
                 static_cast<int>(dx + (iw - nameW) / 2),
                 static_cast<int>(dy + ih + 2),
                 boardNameSize, WHITE);
    }

    float boardRight = boardX + boardTexture.width * boardScale;
    float availW = sw - boardRight - 10.0f;
    float cardScale = std::min(availW / (float)cardPanel.width, (sh - 20.0f) / (float)cardPanel.height);
    if (cardScale < 0.1f) cardScale = 0.1f;
    float cardX = boardRight + (availW - cardPanel.width * cardScale) * 0.65f;
    float cardY = (sh - cardPanel.height * cardScale) / 2.0f;
    DrawTextureEx(cardPanel, {cardX, cardY}, 0.0f, cardScale, WHITE);

    btnPlay.draw();
    btnAssets.draw();
    btnPlayers.draw();
    btnLog.draw();

    float iconSz = cardPanel.width * cardScale * 0.15f;
    float iconStartX = cardX + (cardPanel.width * cardScale - iconSz) - 306.0f;
    float iconY = btnPlay.getY() + btnPlay.getHeight() - 198.0f * globalScale;
    float iconScaleVal = iconSz / (float)playerIcons[currentPlayerIdx].width;
    DrawRectangleRec({iconStartX - 3, iconY - 3, iconSz + 6, iconSz + 6}, {255, 235, 202, 255});
    DrawRectangleLinesEx({iconStartX - 3, iconY - 2, iconSz + 6, iconSz + 6}, 2, {148, 73, 68, 255});
    DrawTextureEx(playerIcons[currentPlayerIdx], {iconStartX, iconY}, 0.0f, iconScaleVal, WHITE);

    float playerNameX = iconStartX + iconSz + 10.0f * globalScale;
    float playerNameY = iconY + (iconSz - 27.0f * globalScale) / 2.0f;
    int playerNameSize = static_cast<int>(35 * globalScale);
    DrawText(gameConfig->playerNames[currentPlayerIdx],
             static_cast<int>(playerNameX), static_cast<int>(playerNameY),
             playerNameSize, BLACK);

    float playerNameBottom = iconY + iconSz + 10.0f * globalScale;
    float diceSz = cardPanel.width * cardScale * 0.10f;
    float diceGap = 10.0f * globalScale;
    float diceY = playerNameBottom + 18.0f * globalScale;

    float totalRowW = diceSz + diceSz + diceGap + btnRollDice.getWidth() + diceGap + btnEndTurn.getWidth();
    float rowStartX = cardX + (cardPanel.width * cardScale - totalRowW) / 2.0f;

    float d1x = rowStartX;
    float d2x = d1x + diceSz + diceGap;
    float dice1Sc = diceSz / (float)diceTextures[dice1 - 1].width;
    float dice2Sc = diceSz / (float)diceTextures[dice2 - 1].width;
    DrawTextureEx(diceTextures[dice1 - 1], {d1x, diceY}, 0.0f, dice1Sc, WHITE);
    DrawTextureEx(diceTextures[dice2 - 1], {d2x, diceY}, 0.0f, dice2Sc, WHITE);

    btnRollDice.draw();
    btnEndTurn.draw();
}