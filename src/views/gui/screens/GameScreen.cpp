#include "views/gui/screens/GameScreen.hpp"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

// ── Tile alignment tuning ────────────────────────────────────────────────────
// Corner tiles width/height as fraction of board w/h  (user confirmed 2/14 ✓)
static const float CORNER_RATIO_W = 2.0f / 14.0f;
static const float CORNER_RATIO_H = 2.0f / 14.0f;

// Board frame/border: fraction of board dimension removed on EACH side before
// computing tile sizes.  Increase if side tiles still appear too long.
// e.g. 0.005 = 0.5% per side  →  tiles shrink ~1% total
static const float BOARD_INSET_W  = 0.002f; // ← tune horizontal tile length
static const float BOARD_INSET_H  = 0.002f; // ← tune vertical   tile length
// ─────────────────────────────────────────────────────────────────────────────

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
      cornerRatioW(CORNER_RATIO_W), cornerRatioH(CORNER_RATIO_H),
      aktaTexture{}, selectedTileIdx(-1),
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

    // effective tile area (excludes board frame)
    float inX  = bw * BOARD_INSET_W;
    float inY  = bh * BOARD_INSET_H;
    float tX0  = boardX + inX;          // left edge of tile area
    float tY0  = boardY + inY;          // top  edge of tile area
    float tW   = bw - 2.0f * inX;      // effective width
    float tH   = bh - 2.0f * inY;      // effective height

    float cornerSz  = tW * cornerRatioW;
    float sideSz    = (tW - 2.0f * cornerSz)  / 9.0f;
    float cornerSzh = tH * cornerRatioH;
    float sideSzh   = (tH - 2.0f * cornerSzh) / 9.0f;

    for (int i = 0; i < 40; i++) {
        int row, col;
        getGridRC(i, row, col);

        float cx, cy;
        if (col == 0)       cx = tX0 + cornerSz / 2.0f;
        else if (col == 10) cx = tX0 + tW - cornerSz / 2.0f;
        else                cx = tX0 + cornerSz + (col - 1) * sideSz + sideSz / 2.0f;

        if (row == 0)        cy = tY0 + cornerSzh / 2.0f;
        else if (row == 10)  cy = tY0 + tH - cornerSzh / 2.0f;
        else                 cy = tY0 + cornerSzh + (row - 1) * sideSzh + sideSzh / 2.0f;

        tileCenters[i][0] = cx;
        tileCenters[i][1] = cy;

        bool isCorner   = (row == 0 || row == 10) && (col == 0 || col == 10);
        bool isTopBot   = (row == 0 || row == 10) && !isCorner;
        float tw = isCorner ? cornerSz  : (isTopBot ? sideSz : cornerSz);
        float th = isCorner ? cornerSzh : (isTopBot ? cornerSzh : sideSzh);
        tileBounds[i] = { cx - tw / 2.0f, cy - th / 2.0f, tw, th };
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

float diceSz = cardPanel.width * cardScale * 0.12f;
    float diceY = playerNameBottom + 18.0f * globalScale;
    float diceGap = 10.0f * globalScale;
    float btnGap = 6.0f * globalScale;

    float rollBtnW = cardPanel.width * cardScale * 0.32f;
    float endBtnW = cardPanel.width * cardScale * 0.15f ;
    float actionBtnH = diceSz * 0.85f;

    float totalW = diceSz + diceSz + diceGap;
    float rowStartX = cardX + (cardPanel.width * cardScale - totalW) / 2.0f - 92.0f * globalScale;

    float dice1X = rowStartX;
    float dice2X = dice1X + diceSz + diceGap;
    float rollBtnX = dice2X + diceSz + diceGap;
    float endBtnX = rollBtnX + rollBtnW + btnGap;

    float actionBtnY = diceY + (diceSz - actionBtnH) + 10.0f/ 2.0f;

    btnRollDice.loadAsText("ROLL", rollBtnX, actionBtnY, rollBtnW, actionBtnH, {244, 206, 43, 255}, {250, 220, 70, 255}, {80, 40, 35, 255});
    btnEndTurn.loadAsText("END", endBtnX, actionBtnY, endBtnW, actionBtnH, {255, 235, 202, 255}, {255, 240, 215, 255}, {80, 40, 35, 255});

    float boxPad   = 12.0f * globalScale;
    float abBoxX   = cardX + boxPad + 20.0f;
    float abBoxY   = btnY + btnH + boxPad + 10.0f;
    float abBoxW   = cardPanel.width * cardScale - 6.0f * boxPad;
    float abBoxH   = 100.0f * globalScale;

    float labelH   = 14.0f * globalScale;
    float abtnStartY = abBoxY + abBoxH + 10.0f * globalScale + labelH + 8.0f * globalScale;
    float abtnColGap = 8.0f * globalScale;
    float abtnRowGap = 6.0f * globalScale;
    float abtnW    = (abBoxW - abtnColGap) / 2.0f;
    float abtnH    = 30.0f * globalScale;

    Color abBg    = {255, 235, 202, 255};
    Color abHover = {255, 220, 180, 255};
    Color abFg    = {80, 40, 35, 255};

    float col2X = abBoxX + abtnW + abtnColGap;
    btnBuyProperty.loadAsText("BUY PROPERTY", abBoxX, abtnStartY,                        abtnW, abtnH, abBg, abHover, abFg);
    btnAuction.loadAsText(   "AUCTION",      col2X,  abtnStartY,                        abtnW, abtnH, abBg, abHover, abFg);
    btnBuildHouse.loadAsText("BUILD HOUSE",  abBoxX, abtnStartY + (abtnH + abtnRowGap), abtnW, abtnH, abBg, abHover, abFg);
    btnMortgage.loadAsText(  "MORTGAGE",     col2X,  abtnStartY + (abtnH + abtnRowGap), abtnW, abtnH, abBg, abHover, abFg);
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

    houseTexture = LoadTexture("assets/assets1/house 1.png");
    hotelTexture = LoadTexture("assets/assets1/hotel 1.png");
    pamTexture   = LoadTexture("assets/assets1/pam 1.png");
    plnTexture   = LoadTexture("assets/assets1/pln 1.png");
    trainTexture = LoadTexture("assets/assets1/train 1.png");
    aktaTexture  = LoadTexture("assets/Akta_background.png");

    std::string line;
    {
        std::ifstream f("config/board.txt");
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ss(line);
            int pos; std::string code, name, type;
            ss >> pos >> code >> name >> type;
            if (pos < 1 || pos > 40) continue;
            for (char& c : name) if (c == '_') c = ' ';
            int idx = pos - 1;
            tileData[idx].name = name;
            tileData[idx].code = code;
            tileData[idx].type = type;
        }
    }
    {
        std::ifstream f("config/property.txt");
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ss(line);
            int pos; std::string code, name, type, color;
            ss >> pos >> code >> name >> type >> color;
            if (pos < 1 || pos > 40) continue;
            for (char& c : color) if (c == '_') c = ' ';
            int idx = pos - 1;
            tileData[idx].colorGroup = color;
            if (type == "STREET") {
                ss >> tileData[idx].buyPrice >> tileData[idx].mortgageValue
                   >> tileData[idx].houseCost >> tileData[idx].hotelCost;
                for (int r = 0; r < 6; r++) ss >> tileData[idx].rent[r];
            } else {
                int dummy; ss >> dummy >> tileData[idx].mortgageValue;
            }
        }
    }

    computeLayout();
}

void GameScreen::unloadAssets() {
    UnloadTexture(bgTexture);
    UnloadTexture(blurredBgTexture);
    UnloadTexture(boardTexture);
    UnloadTexture(cardPanel);
    for (int i = 0; i < 6; i++) UnloadTexture(playerIcons[i]);
    for (int i = 0; i < 6; i++) UnloadTexture(diceTextures[i]);
    UnloadTexture(houseTexture);
    UnloadTexture(hotelTexture);
    UnloadTexture(pamTexture);
    UnloadTexture(plnTexture);
    UnloadTexture(trainTexture);
    UnloadTexture(aktaTexture);
    btnPlay.unload();
    btnAssets.unload();
    btnPlayers.unload();
    btnLog.unload();
    btnRollDice.unload();
    btnEndTurn.unload();
    btnBuyProperty.unload();
    btnAuction.unload();
    btnBuildHouse.unload();
    btnMortgage.unload();
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
        if (selectedTileIdx >= 0) {
            selectedTileIdx = -1;
        } else {
            nextScreen = AppScreen::HOME;
            shouldChangeScreen = true;
        }
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mouse = GetMousePosition();
        if (selectedTileIdx >= 0) {
            selectedTileIdx = -1;
        } else {
            for (int i = 0; i < 40; i++) {
                if (CheckCollisionPointRec(mouse, tileBounds[i])) {
                    selectedTileIdx = i;
                    break;
                }
            }
        }
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

    // hitboxes invisible — used only for click detection

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

    if (activeTab == 0) {
        float boxPad = 12.0f * globalScale;
        float boxX = cardX + boxPad + 20.0f;
        float boxY = btnPlay.getY() + btnPlay.getHeight() + boxPad + 10.0f;
        float boxW = cardPanel.width * cardScale - 6.0f * boxPad;
        float boxH = 100.0f * globalScale;

        DrawRectangleRec({boxX, boxY, boxW, boxH}, {255, 243, 210, 255});
        DrawRectangleLinesEx({boxX, boxY, boxW, boxH}, 2.5f, {50, 25, 20, 255});

        int titleSz = static_cast<int>(15 * globalScale);
        int subSz   = static_cast<int>(12 * globalScale);

        if (diceRolling) {
            DrawText("ROLLING...",
                     static_cast<int>(boxX + 8 * globalScale),
                     static_cast<int>(boxY + 8 * globalScale),
                     titleSz, BLACK);
        } else if (hasRolled) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%d + %d = %d", dice1, dice2, dice1 + dice2);
            DrawText("DICE RESULT",
                     static_cast<int>(boxX + 8 * globalScale),
                     static_cast<int>(boxY + 8 * globalScale),
                     titleSz, BLACK);
            DrawText(buf,
                     static_cast<int>(boxX + 8 * globalScale),
                     static_cast<int>(boxY + 8 * globalScale + titleSz + 4 * globalScale),
                     subSz, {80, 40, 35, 255});
        } else {
            DrawText("Roll the dice to play!",
                     static_cast<int>(boxX + 8 * globalScale),
                     static_cast<int>(boxY + 16 * globalScale),
                     subSz, {120, 80, 75, 255});
        }

        float actionsLabelY = boxY + boxH + 10.0f * globalScale;
        int labelSz = static_cast<int>(13 * globalScale);
        DrawText("ACTIONS",
                 static_cast<int>(boxX),
                 static_cast<int>(actionsLabelY),
                 labelSz, {148, 73, 68, 255});

        Color border = {80, 40, 35, 255};
        auto drawActionBtn = [&](Button& btn) {
            btn.draw();
            DrawRectangleLinesEx({btn.getX(), btn.getY(), btn.getWidth(), btn.getHeight()}, 1.5f, border);
        };
        drawActionBtn(btnBuyProperty);
        drawActionBtn(btnAuction);
        drawActionBtn(btnBuildHouse);
        drawActionBtn(btnMortgage);
    }

    if (activeTab == 2) {
        static const Color playerCardColors[6] = {
            {200, 60,  60,  255},
            {70,  120, 200, 255},
            {70,  160, 90,  255},
            {220, 140, 50,  255},
            {140, 80,  180, 255},
            {50,  170, 170, 255},
        };

        float pad     = 12.0f * globalScale;
        float listX   = cardX + pad + 20.0f;
        float listY   = btnPlay.getY() + btnPlay.getHeight() + pad + 10.0f;
        float listW   = cardPanel.width * cardScale - 6.0f * pad;
        float cardH   = 58.0f * globalScale;
        float cardGap = 5.0f * globalScale;
        float iconBox = cardH - 8.0f * globalScale;

        int nameSz  = static_cast<int>(15 * globalScale);
        int statSz  = static_cast<int>(15 * globalScale);
        float statIconH = static_cast<float>(statSz + 4) * globalScale;

        int count = gameConfig->playerCount;
        for (int i = 0; i < count; i++) {
            float cy = listY + i * (cardH + cardGap);

            Color bg = (i == currentPlayerIdx)
                       ? Color{255, 243, 160, 255}
                       : Color{255, 243, 210, 255};
            DrawRectangleRec({listX, cy, listW, cardH}, bg);
            DrawRectangleLinesEx({listX, cy, listW, cardH}, 2.0f, {50, 25, 20, 255});

            float iconBoxX = listX + 4.0f * globalScale;
            float iconBoxY = cy + (cardH - iconBox) / 2.0f;
            DrawRectangleRec({iconBoxX, iconBoxY, iconBox, iconBox}, playerCardColors[i % 6]);
            float iconSc = iconBox / (float)playerIcons[i].width;
            DrawTextureEx(playerIcons[i], {iconBoxX, iconBoxY}, 0.0f, iconSc, WHITE);

            float textX = iconBoxX + iconBox + 8.0f * globalScale;
            float nameY = cy + 8.0f * globalScale;
            DrawText(gameConfig->playerNames[i], static_cast<int>(textX), static_cast<int>(nameY), nameSz, BLACK);

            if (i == currentPlayerIdx) {
                int nw = MeasureText(gameConfig->playerNames[i], nameSz);
                int turnSz = static_cast<int>(10 * globalScale);
                DrawText("- turn",
                         static_cast<int>(textX + nw + 8 * globalScale),
                         static_cast<int>(nameY + 1 * globalScale),
                         turnSz, {148, 73, 68, 255});
            }

            float statsY = nameY + nameSz + 4.0f * globalScale;
            float cx = textX;

            auto drawStat = [&](Texture2D& tex, int count) {
                char buf[8];
                snprintf(buf, sizeof(buf), "%d", count);
                float sc = statIconH / (float)tex.height;
                DrawTextureEx(tex, {cx, statsY - 1.0f * globalScale}, 0.0f, sc, WHITE);
                cx += tex.width * sc + 2.0f * globalScale;
                DrawText(buf, static_cast<int>(cx), static_cast<int>(statsY), statSz, BLACK);
                cx += MeasureText(buf, statSz) + 8.0f * globalScale;
            };

            char moneyBuf[16];
            snprintf(moneyBuf, sizeof(moneyBuf), "$1,500");
            DrawText(moneyBuf, static_cast<int>(cx), static_cast<int>(statsY), statSz, BLACK);
            cx += MeasureText(moneyBuf, statSz) + 8.0f * globalScale;

            drawStat(houseTexture, 2);
            drawStat(hotelTexture, 1);
            drawStat(pamTexture,   1);
            drawStat(plnTexture,   1);
            float sc = statIconH / (float)trainTexture.height;
            DrawTextureEx(trainTexture, {cx, statsY - 1.0f * globalScale}, 0.0f, sc, WHITE);
            cx += trainTexture.width * sc + 2.0f * globalScale;
            DrawText("1", static_cast<int>(cx), static_cast<int>(statsY), statSz, BLACK);
        }
    }

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
    float diceSz = cardPanel.width * cardScale * 0.14f;
    float diceGap = 10.0f * globalScale;
    float diceY = playerNameBottom + 21.0f * globalScale;
    float btnGap = 6.0f * globalScale;

    float totalW = diceSz + diceSz + diceGap;
    float d1x = cardX + (cardPanel.width * cardScale - totalW) / 2.0f - 105.0f * globalScale;
    float d2x = d1x + diceSz + diceGap;
    float dice1Sc = diceSz / (float)diceTextures[dice1 - 1].width;
    float dice2Sc = diceSz / (float)diceTextures[dice2 - 1].width;
    DrawTextureEx(diceTextures[dice1 - 1], {d1x, diceY}, 0.0f, dice1Sc, WHITE);
    DrawTextureEx(diceTextures[dice2 - 1], {d2x, diceY}, 0.0f, dice2Sc, WHITE);

    btnRollDice.draw();
    DrawRectangleLinesEx({btnRollDice.getX(), btnRollDice.getY(), btnRollDice.getWidth(), btnRollDice.getHeight()}, 2, BLACK);
    btnEndTurn.draw();

    // ── Akta overlay ─────────────────────────────────────────────────────────
    if (selectedTileIdx >= 0) {
        float bw = boardTexture.width * boardScale;
        float bh = boardTexture.height * boardScale;

        DrawRectangle((int)boardX, (int)boardY, (int)bw, (int)bh, {0, 0, 0, 120});

        float aktaScale = bw * 0.42f / (float)aktaTexture.width;
        float aktaW = aktaTexture.width  * aktaScale;
        float aktaH = aktaTexture.height * aktaScale;
        float aktaX = boardX + (bw - aktaW) / 2.0f;
        float aktaY = boardY + (bh - aktaH) / 2.0f;
        DrawTextureEx(aktaTexture, {aktaX, aktaY}, 0.0f, aktaScale, WHITE);

        const TileInfo& info = tileData[selectedTileIdx];

        // ── Header: name + type ───────────────────────────────────────────────
        int nameSz = static_cast<int>(aktaH * 0.055f);
        int nameW  = MeasureText(info.name.c_str(), nameSz);
        DrawText(info.name.c_str(),
                 (int)(aktaX + (aktaW - nameW) / 2.0f),
                 (int)(aktaY + aktaH * 0.04f),
                 nameSz, BLACK);

        int typeSz = static_cast<int>(aktaH * 0.030f);
        int typeW  = MeasureText(info.type.c_str(), typeSz);
        DrawText(info.type.c_str(),
                 (int)(aktaX + (aktaW - typeW) / 2.0f),
                 (int)(aktaY + aktaH * 0.13f),
                 typeSz, {148, 73, 68, 255});

        // ── Body: rows ────────────────────────────────────────────────────────
        float lx  = aktaX + aktaW * 0.08f;           // left  text margin
        float rx  = aktaX + aktaW * 0.92f;            // right text anchor
        float ry  = aktaY + aktaH * 0.23f;            // current row Y
        float rH  = aktaH * 0.052f;                   // row height
        int   rSz = static_cast<int>(aktaH * 0.038f); // row font size
        int   lSz = static_cast<int>(aktaH * 0.033f); // label font size
        Color lC  = {80,  50, 40, 255};               // label colour
        Color vC  = {30,  20, 15, 255};               // value colour
        Color sC  = {148, 73, 68, 255};               // section title colour

        char buf[64];

        auto drawRow = [&](const char* label, const char* value) {
            DrawText(label, (int)lx, (int)ry, rSz, lC);
            int vw = MeasureText(value, rSz);
            DrawText(value, (int)(rx - vw), (int)ry, rSz, vC);
            ry += rH;
        };
        auto drawDivider = [&]() {
            ry += rH * 0.1f;
            DrawLineEx({lx, ry}, {rx, ry}, 0.8f, {160, 100, 80, 120});
            ry += rH * 0.4f;
        };
        auto drawSection = [&](const char* title) {
            DrawText(title, (int)lx, (int)ry, lSz, sC);
            ry += rH * 0.75f;
        };

        if (info.type == "STREET") {
            snprintf(buf, sizeof(buf), "Rp %d", info.buyPrice);
            drawRow("Harga Beli",  buf);
            snprintf(buf, sizeof(buf), "Rp %d", info.mortgageValue);
            drawRow("Nilai Gadai", buf);

            drawDivider();
            drawSection("SEWA");
            const char* rentLabels[6] = {"Lahan","1 Rumah","2 Rumah","3 Rumah","4 Rumah","Hotel"};
            for (int r = 0; r < 6; r++) {
                snprintf(buf, sizeof(buf), "Rp %d", info.rent[r]);
                drawRow(rentLabels[r], buf);
            }
            drawDivider();
            drawSection("BIAYA PEMBANGUNAN");
            snprintf(buf, sizeof(buf), "Rp %d", info.houseCost);
            drawRow("Rumah", buf);
            snprintf(buf, sizeof(buf), "Rp %d", info.hotelCost);
            drawRow("Hotel", buf);

        } else if (info.type == "RAILROAD") {
            snprintf(buf, sizeof(buf), "Rp %d", info.mortgageValue);
            drawRow("Nilai Gadai", buf);
            drawRow("Cara Beli",   "Gratis (mendarat)");

            drawDivider();
            drawSection("SEWA");
            const int railRent[4] = {25, 50, 100, 200};
            const char* railLabels[4] = {"1 Stasiun","2 Stasiun","3 Stasiun","4 Stasiun"};
            for (int r = 0; r < 4; r++) {
                snprintf(buf, sizeof(buf), "Rp %d", railRent[r]);
                drawRow(railLabels[r], buf);
            }

        } else if (info.type == "UTILITY") {
            snprintf(buf, sizeof(buf), "Rp %d", info.mortgageValue);
            drawRow("Nilai Gadai", buf);
            drawRow("Cara Beli",   "Gratis (mendarat)");

            drawDivider();
            drawSection("SEWA");
            drawRow("1 Utilitas", "Dadu x 4");
            drawRow("2 Utilitas", "Dadu x 10");

        } else {
            // Non-purchasable tiles (GO, Jail, Tax, etc.)
            drawRow("Tipe", info.type.c_str());
        }

        // ── Hint ─────────────────────────────────────────────────────────────
        int hintSz = static_cast<int>(aktaH * 0.028f);
        const char* hint = "Click anywhere to close";
        int hintW = MeasureText(hint, hintSz);
        DrawText(hint,
                 (int)(aktaX + (aktaW - hintW) / 2.0f),
                 (int)(aktaY + aktaH * 0.90f),
                 hintSz, {160, 120, 100, 160});
    }
}