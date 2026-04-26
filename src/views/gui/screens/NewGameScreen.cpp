#include "views/gui/screens/NewGameScreen.hpp"
#include <cmath>
#include <cstring>

NewGameScreen::NewGameScreen(GameConfig& config) : bgTexture{}, playerCountLabel{}, selectIconsLabel{},
    minMaxLabel{}, counterTex{},
    playerCount(2), iconSelected{false}, globalScale(1.0f),
    gameConfig(&config), activeNameField(-1) {
    for (int i = 0; i < 4; i++) playerIcons[i] = {};
    iconSelected[0] = true;
    iconSelected[1] = true;
    config.playerCount = playerCount;
    for (int i = 0; i < GameConfig::MAX_PLAYERS; i++) {
        nameCursorPos[i] = (int)std::strlen(config.playerNames[i]);
    }
}

void NewGameScreen::loadAssets() {
    TraceLog(LOG_INFO, "NGS: loading bg...");
    bgTexture = LoadTexture("assets/page_background.png");
    TraceLog(LOG_INFO, "NGS: loading labels...");
    playerCountLabel = LoadTexture("assets/new-game-page/PLAYER COUNT_.png");
    selectIconsLabel = LoadTexture("assets/new-game-page/SELECT ICONS_.png");
    minMaxLabel = LoadTexture("assets/new-game-page/(MIN_ 2, MAX_ 6).png");
    counterTex = LoadTexture("assets/new-game-page/counter 2.png");
    TraceLog(LOG_INFO, "NGS: loading icons...");
    playerIcons[0] = LoadTexture("assets/assets1/gru_b.png");
    playerIcons[1] = LoadTexture("assets/assets1/minion_b.png");
    playerIcons[2] = LoadTexture("assets/assets1/purple_b.png");
    playerIcons[3] = LoadTexture("assets/assets1/banana_b.png");
    TraceLog(LOG_INFO, "NGS: loading buttons...");
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    globalScale = std::min(sw / 1280.0f, sh / 720.0f);

    const float TARGET_BTN_H = 55.0f;
    float btnY = sh * 0.84f;

    btnBack.load("assets/new-game-page/back 2.png", 0, btnY + 2.0f * globalScale, 1.0f);
    btnBack.setScale(globalScale * 48.2f/ btnBack.getHeight());

    btnStart.load("assets/new-game-page/start 2.png", 0, btnY + 2.0f * globalScale, 1.0f);
    btnStart.setScale(TARGET_BTN_H * globalScale / btnStart.getHeight());

    float gap = 30.0f * globalScale;
    float totalW = btnBack.getWidth() + gap + btnStart.getWidth();
    float groupX = (sw - totalW) / 2.0f;
    btnBack.setPosition(groupX, btnY - 57.0f * globalScale);
    btnStart.setPosition(groupX + btnBack.getWidth() + gap, btnY - 62.5f * globalScale);
    TraceLog(LOG_INFO, "NGS: buttons loaded");
}

void NewGameScreen::unloadAssets() {
    UnloadTexture(bgTexture);
    UnloadTexture(playerCountLabel);
    UnloadTexture(selectIconsLabel);
    UnloadTexture(minMaxLabel);
    UnloadTexture(counterTex);
    for (int i = 0; i < 4; i++) UnloadTexture(playerIcons[i]);
    btnBack.unload();
    btnStart.unload();
    btnPlus.unload();
    btnMinus.unload();
}

void NewGameScreen::update(float dt) {
    (void)dt;

    if (btnBack.isClicked()) {
        nextScreen = AppScreen::HOME;
        shouldChangeScreen = true;
        return;
    } else if (btnStart.isClicked()) {
        nextScreen = AppScreen::GAME;
        shouldChangeScreen = true;
        return;
    }

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    float boxW = 90 * globalScale, boxH = 55 * globalScale;
    float counterX = (sw - boxW) / 2.0f;
    float counterY = sh * 0.29f;
    float btnBoxW = 44 * globalScale, btnBoxH = 38 * globalScale;
    float btnBoxY = counterY + (boxH - btnBoxH) / 2.0f;
    float minusX = counterX - btnBoxW - 14 * globalScale;
    float plusX  = counterX + boxW + 14 * globalScale;

    Vector2 mouse = GetMousePosition();
    Rectangle plusRect  = {plusX,  btnBoxY, btnBoxW, btnBoxH};
    Rectangle minusRect = {minusX, btnBoxY, btnBoxW, btnBoxH};

    if (CheckCollisionPointRec(mouse, plusRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (playerCount < 4) {
            playerCount++;
            gameConfig->playerCount = playerCount;
            activeNameField = -1;
        }
    }
    if (CheckCollisionPointRec(mouse, minusRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (playerCount > 2) {
            playerCount--;
            gameConfig->playerCount = playerCount;
            activeNameField = -1;
        }
    }

    float iconScale = globalScale * 1.0f;
    float iconW = playerIcons[0].width * iconScale;
    float iconH = playerIcons[0].height * iconScale;
    float iconGap = 28 * globalScale;

    float nameFieldW = 130 * globalScale;
    float nameFieldH = 30 * globalScale;
    float cellW = std::max(iconW, nameFieldW);
    float nameFieldGapY = 6 * globalScale;

    float iconStartX = (sw - (playerCount * cellW + (playerCount - 1) * iconGap)) / 2.0f;
    float iconY = sh * 0.45f;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        activeNameField = -1;
        for (int i = 0; i < playerCount; i++) {
            float cx = iconStartX + i * (cellW + iconGap) + cellW / 2.0f;
            float nfX = cx - nameFieldW / 2.0f;
            float nfY = iconY + iconH + nameFieldGapY;
            Rectangle nameRect = {nfX, nfY, nameFieldW, nameFieldH};
            if (CheckCollisionPointRec(mouse, nameRect)) {
                activeNameField = i;
                nameCursorPos[i] = (int)std::strlen(gameConfig->playerNames[i]);
            }
        }
    }

    if (activeNameField >= 0 && activeNameField < playerCount) {
        int idx = activeNameField;
        int key = GetCharPressed();
        int len = (int)std::strlen(gameConfig->playerNames[idx]);
        while (key > 0) {
            if (len < GameConfig::MAX_NAME_LEN && key > 32 && key <= 125) {
                gameConfig->playerNames[idx][len] = (char)key;
                gameConfig->playerNames[idx][len + 1] = '\0';
                len++;
                nameCursorPos[idx] = len;
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            if (len > 0) {
                gameConfig->playerNames[idx][len - 1] = '\0';
                nameCursorPos[idx] = len - 1;
            }
        }
    }

    if (IsWindowResized()) {
        unloadAssets();
        loadAssets();
    }
}

void NewGameScreen::draw() {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    float bgScale = std::max(sw / (float)bgTexture.width, sh / (float)bgTexture.height);
    float bgX = (sw - bgTexture.width * bgScale) / 2.0f;
    float bgY = (sh - bgTexture.height * bgScale) / 2.0f;
    DrawTextureEx(bgTexture, {bgX, bgY}, 0.0f, bgScale, WHITE);

    float labelScale = globalScale * 0.5f;
    DrawTextureEx(playerCountLabel, {(sw - playerCountLabel.width * labelScale) / 2.0f, sh * 0.13f}, 0.0f, labelScale, WHITE);

    int mmFontSize = static_cast<int>(22 * globalScale);
    const char* mmText = "(MIN: 2, MAX: 4)";
    int mmW = MeasureText(mmText, mmFontSize);
    DrawText(mmText, (sw - mmW) / 2, static_cast<int>(sh * 0.21f), mmFontSize, YELLOW);

    float boxW = 90 * globalScale;
    float boxH = 55 * globalScale;
    float counterX = (sw - boxW) / 2.0f;
    float counterY = sh * 0.29f;
    DrawRectangleRounded({counterX, counterY, boxW, boxH}, 0.2f, 8, Fade(BLACK, 0.6f));
    DrawRectangleRoundedLines({counterX, counterY, boxW, boxH}, 0.2f, 8, YELLOW);

    int fontSize = static_cast<int>(38 * globalScale);
    const char* countText = TextFormat("%d", playerCount);
    int textW = MeasureText(countText, fontSize);
    DrawText(countText,
             static_cast<int>(counterX + (boxW - textW) / 2),
             static_cast<int>(counterY + (boxH - fontSize) / 2),
             fontSize, WHITE);

    float btnBoxW = 44 * globalScale, btnBoxH = 38 * globalScale;
    float minusX = counterX - btnBoxW - 14 * globalScale;
    float plusX  = counterX + boxW + 14 * globalScale;
    float btnBoxY = counterY + (boxH - btnBoxH) / 2.0f;

    DrawRectangleRounded({minusX, btnBoxY, btnBoxW, btnBoxH}, 0.3f, 8, Fade(BLACK, 0.5f));
    DrawRectangleRoundedLines({minusX, btnBoxY, btnBoxW, btnBoxH}, 0.3f, 8, YELLOW);
    int pmSize = static_cast<int>(30 * globalScale);
    int mW = MeasureText("-", pmSize);
    DrawText("-", static_cast<int>(minusX + (btnBoxW - mW) / 2), static_cast<int>(btnBoxY + (btnBoxH - pmSize) / 2 - 2), pmSize, WHITE);

    DrawRectangleRounded({plusX, btnBoxY, btnBoxW, btnBoxH}, 0.3f, 8, Fade(BLACK, 0.5f));
    DrawRectangleRoundedLines({plusX, btnBoxY, btnBoxW, btnBoxH}, 0.3f, 8, YELLOW);
    int pW = MeasureText("+", pmSize);
    DrawText("+", static_cast<int>(plusX + (btnBoxW - pW) / 2), static_cast<int>(btnBoxY + (btnBoxH - pmSize) / 2 - 2), pmSize, WHITE);

    float iconScale = globalScale * 1.0f;
    float iconW = playerIcons[0].width * iconScale;
    float iconH = playerIcons[0].height * iconScale;
    float iconGap = 28 * globalScale;
    float nameFieldW = 130 * globalScale;
    float nameFieldH = 30 * globalScale;
    float cellW = std::max(iconW, nameFieldW);
    float nameFieldGapY = 6 * globalScale;

    float iconStartX = (sw - (playerCount * cellW + (playerCount - 1) * iconGap)) / 2.0f;
    float iconY = sh * 0.45f;

    int nameFontSize = static_cast<int>(20 * globalScale);

    for (int i = 0; i < playerCount; i++) {
        float cx = iconStartX + i * (cellW + iconGap) + cellW / 2.0f;
        float ix = cx - iconW / 2.0f;
        DrawTextureEx(playerIcons[i], {ix, iconY}, 0.0f, iconScale, WHITE);

        float nfX = cx - nameFieldW / 2.0f;
        float nfY = iconY + iconH + nameFieldGapY;
        nameFields[i] = {nfX, nfY, nameFieldW, nameFieldH};

        Color fieldBg = (activeNameField == i) ? Fade(WHITE, 0.25f) : Fade(BLACK, 0.35f);
        Color fieldBorder = (activeNameField == i) ? YELLOW : Fade(YELLOW, 0.5f);
        DrawRectangleRounded(nameFields[i], 0.15f, 8, fieldBg);
        DrawRectangleRoundedLines(nameFields[i], 0.15f, 8, fieldBorder);

        const char* nameText = gameConfig->playerNames[i];
        int textW = MeasureText(nameText, nameFontSize);
        int textX = static_cast<int>(nfX + nameFieldW / 2 - textW / 2);
        int textY = static_cast<int>(nfY + (nameFieldH - nameFontSize) / 2);
        DrawText(nameText, textX, textY, nameFontSize, WHITE);

        if (activeNameField == i) {
            int blinkOn = (GetTime() - (int)GetTime()) < 0.5 ? 1 : 0;
            if (blinkOn) {
                int cursorX = textX + textW;
                DrawLine(cursorX, static_cast<int>(nfY + 4 * globalScale),
                         cursorX, static_cast<int>(nfY + nameFieldH - 4 * globalScale), WHITE);
            }
        }
    }

    btnBack.draw();
    btnStart.draw();
}
