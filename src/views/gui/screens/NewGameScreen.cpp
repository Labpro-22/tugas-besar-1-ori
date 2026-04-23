#include "NewGameScreen.hpp"
#include <cmath>

NewGameScreen::NewGameScreen() : bgTexture{}, playerCountLabel{}, selectIconsLabel{},
    minMaxLabel{}, counterTex{},
    playerCount(2), iconSelected{false}, globalScale(1.0f) {
    for (int i = 0; i < 6; i++) playerIcons[i] = {};
    iconSelected[0] = true;
    iconSelected[1] = true;
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
    playerIcons[0] = LoadTexture("assets/assets1/icon-1.png");
    playerIcons[1] = LoadTexture("assets/assets1/icon-2.png");
    playerIcons[2] = LoadTexture("assets/assets1/icon-3.png");
    playerIcons[3] = LoadTexture("assets/assets1/icon-4.png");
    playerIcons[4] = LoadTexture("assets/assets1/icon-5.png");
    playerIcons[5] = LoadTexture("assets/assets1/icon-6.png");
    TraceLog(LOG_INFO, "NGS: loading buttons...");
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    globalScale = std::min(sw / 1280.0f, sh / 720.0f);

    // back 2.png=302x64, start 2.png=413x96 — normalize both to same on-screen height
    const float TARGET_BTN_H = 55.0f;
    float btnY = sh * 0.84f;

    btnBack.load("assets/new-game-page/back 2.png", 0, btnY + 2.0f, 1.0f);
    btnBack.setScale(globalScale * 50.0f/ btnBack.getHeight());

    btnStart.load("assets/new-game-page/start 2.png", 0, btnY + 2.0f, 1.0f);
    btnStart.setScale(TARGET_BTN_H * globalScale / btnStart.getHeight());

    float gap = 30.0f * globalScale;
    float totalW = btnBack.getWidth() + gap + btnStart.getWidth();
    float groupX = (sw - totalW) / 2.0f;
    btnBack.setPosition(groupX, btnY - 58.0f);
    btnStart.setPosition(groupX + btnBack.getWidth() + gap, btnY - 62.5f);
    TraceLog(LOG_INFO, "NGS: buttons loaded");
}

void NewGameScreen::unloadAssets() {
    UnloadTexture(bgTexture);
    UnloadTexture(playerCountLabel);
    UnloadTexture(selectIconsLabel);
    UnloadTexture(minMaxLabel);
    UnloadTexture(counterTex);
    for (int i = 0; i < 6; i++) UnloadTexture(playerIcons[i]);
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

    // Counter geometry — must match draw()
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
        if (playerCount < 4) playerCount++;
    }
    if (CheckCollisionPointRec(mouse, minusRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (playerCount > 2) playerCount--;
    }

    float iconScale = globalScale * 0.22f;
    float iconW = playerIcons[0].width * iconScale;
    float iconH = playerIcons[0].height * iconScale;
    float iconGap = 28 * globalScale;
    float iconStartX = (sw - (playerCount * iconW + (playerCount - 1) * iconGap)) / 2.0f;
    float iconY = sh * 0.45f;

    // Icon clicking removed — icons are decorative only

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

    // Min/max: draw text directly (no asset for MAX 4)
    int mmFontSize = static_cast<int>(22 * globalScale);
    const char* mmText = "(MIN: 2, MAX: 4)";
    int mmW = MeasureText(mmText, mmFontSize);
    DrawText(mmText, (sw - mmW) / 2, static_cast<int>(sh * 0.21f), mmFontSize, YELLOW);

    // Counter: draw a custom box + number instead of using the spinner asset
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

    // Minus button (left of box)
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
    float iconStartX = (sw - (playerCount * iconW + (playerCount - 1) * iconGap)) / 2.0f;
    float iconY = sh * 0.45f;

    for (int i = 0; i < playerCount; i++) {
        float ix = iconStartX + i * (iconW + iconGap);
        DrawTextureEx(playerIcons[i], {ix, iconY}, 0.0f, iconScale, WHITE);
        const char* pName = TextFormat("P%d", i + 1);
        int nameSize = static_cast<int>(18 * globalScale);
        int nameW = MeasureText(pName, nameSize);
        DrawText(pName, static_cast<int>(ix + (iconW - nameW) / 2),
                 static_cast<int>(iconY + iconH + 8),
                 nameSize, WHITE);
    }

    btnBack.draw();
    btnStart.draw();
}
