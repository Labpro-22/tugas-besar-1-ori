#include "views/gui/screens/SettingsScreen.hpp"
#include <cmath>

SettingsScreen::SettingsScreen()
    : bgTexture{}, musicLabel{}, sfxLabel{}, ellipseOn{}, ellipseOff{},
      musicEnabled(true), sfxEnabled(true), globalScale(1.0f) {}

void SettingsScreen::loadAssets() {
    bgTexture = LoadTexture("assets/page_background.png");
    musicLabel = LoadTexture("assets/settings/MUSIC.png");
    sfxLabel = LoadTexture("assets/settings/SFX.png");
    ellipseOn = LoadTexture("assets/settings/Ellipse 1.png");
    ellipseOff = LoadTexture("assets/settings/Ellipse 2.png");

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    globalScale = std::min(sw / 1280.0f, sh / 720.0f);

    float btnScale = globalScale * 0.4f;
    btnBack.loadWithHover("assets/new-game-page/back 2.png", "assets/assets1/back 1.png",
                           sw * 0.05f, sh * 0.85f, btnScale);
}

void SettingsScreen::unloadAssets() {
    UnloadTexture(bgTexture);
    UnloadTexture(musicLabel);
    UnloadTexture(sfxLabel);
    UnloadTexture(ellipseOn);
    UnloadTexture(ellipseOff);
    btnBack.unload();
}

void SettingsScreen::update(float dt) {
    (void)dt;

    if (btnBack.isClicked() || IsKeyPressed(KEY_ESCAPE)) {
        nextScreen = AppScreen::GAME;
        shouldChangeScreen = true;
        return;
    }

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    float labelScale = globalScale * 0.6f;
    float toggleScale = globalScale * 0.4f;
    float labelW = musicLabel.width * labelScale;
    float startX = (sw - labelW - 20 * globalScale - ellipseOn.width * toggleScale) / 2.0f;
    float musicY = sh * 0.35f;
    float sfxY = sh * 0.50f;

    Vector2 mouse = GetMousePosition();
    Rectangle musicRect = {startX + labelW + 20 * globalScale, musicY,
                           ellipseOn.width * toggleScale, ellipseOn.height * toggleScale};
    Rectangle sfxRect = {startX + labelW + 20 * globalScale, sfxY,
                         ellipseOn.width * toggleScale, ellipseOn.height * toggleScale};

    if (CheckCollisionPointRec(mouse, musicRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        musicEnabled = !musicEnabled;
    }
    if (CheckCollisionPointRec(mouse, sfxRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        sfxEnabled = !sfxEnabled;
    }

    if (IsWindowResized()) {
        unloadAssets();
        loadAssets();
    }
}

void SettingsScreen::draw() {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    float bgScale = std::max(sw / (float)bgTexture.width, sh / (float)bgTexture.height);
    float bgX = (sw - bgTexture.width * bgScale) / 2.0f;
    float bgY = (sh - bgTexture.height * bgScale) / 2.0f;
    DrawTextureEx(bgTexture, {bgX, bgY}, 0.0f, bgScale, WHITE);

    float labelScale = globalScale * 0.6f;
    float toggleScale = globalScale * 0.4f;
    float labelW = musicLabel.width * labelScale;
    float startX = (sw - labelW - 20 * globalScale - ellipseOn.width * toggleScale) / 2.0f;
    float musicY = sh * 0.35f;
    float sfxY = sh * 0.50f;

    const char* title = "SETTINGS";
    int titleSize = static_cast<int>(48 * globalScale);
    int titleW = MeasureText(title, titleSize);
    DrawText(title, (sw - titleW) / 2, static_cast<int>(sh * 0.15f), titleSize, GOLD);

    DrawTextureEx(musicLabel, {startX, musicY}, 0.0f, labelScale, WHITE);
    DrawTextureEx(musicEnabled ? ellipseOn : ellipseOff,
                  {startX + labelW + 20 * globalScale, musicY}, 0.0f, toggleScale, WHITE);

    DrawTextureEx(sfxLabel, {startX, sfxY}, 0.0f, labelScale, WHITE);
    DrawTextureEx(sfxEnabled ? ellipseOn : ellipseOff,
                  {startX + labelW + 20 * globalScale, sfxY}, 0.0f, toggleScale, WHITE);

    int statusSize = static_cast<int>(18 * globalScale);
    DrawText(musicEnabled ? "ON" : "OFF",
             static_cast<int>(startX + labelW + 20 * globalScale + ellipseOn.width * toggleScale + 15 * globalScale),
             static_cast<int>(musicY + (ellipseOn.height * toggleScale - statusSize) / 2),
             statusSize, musicEnabled ? GREEN : RED);
    DrawText(sfxEnabled ? "ON" : "OFF",
             static_cast<int>(startX + labelW + 20 * globalScale + ellipseOn.width * toggleScale + 15 * globalScale),
             static_cast<int>(sfxY + (ellipseOn.height * toggleScale - statusSize) / 2),
             statusSize, sfxEnabled ? GREEN : RED);

    btnBack.draw();
}
