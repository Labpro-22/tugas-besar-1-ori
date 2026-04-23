#include "HomeScreen.hpp"
#include <cmath>

HomeScreen::HomeScreen() : bgTexture{}, titleTexture{}, titleScale(0.5f), titleX(0), titleY(0),
    showLoadGameMsg(false), msgTimer(0.0f) {}

void HomeScreen::loadAssets() {
    bgTexture = LoadTexture("assets/page_background.png");
    titleTexture = LoadTexture("assets/home_page/title.png");

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    titleScale = std::min(sw / (float)titleTexture.width * 0.7f, sh / (float)titleTexture.height * 0.3f);
    titleX = (sw - titleTexture.width * titleScale) / 2.0f;
    titleY = sh * 0.12f;

    // Button images (707x707 / 707x566) have large transparent padding.
    // Actual content bbox (from image analysis):
    //   new-game / load-game: content y=298–375 (h=78) in 707x707
    //   exit:                 content y=300–378 (h=79) in 707x566
    // We scale so visible content = TARGET_H on screen, then stack images
    // by content height + gap (images overlap their transparent regions).
    const float TARGET_H = 60.0f;            // desired on-screen button height (px at 720p)
    const float IMG_CONTENT_H = 78.0f;       // content height in source pixels
    const float IMG_TOP_PAD   = 298.0f;      // transparent pixels above content
    const float IMG_W_SRC     = 707.0f;
    const float EXIT_TOP_PAD  = 300.0f;

    float gs = std::min(sw / 1280.0f, sh / 720.0f);
    float btnScale   = TARGET_H * gs / IMG_CONTENT_H;   // maps content to TARGET_H
    float contentH   = IMG_CONTENT_H * btnScale;         // ≈ TARGET_H * gs
    float topPad     = IMG_TOP_PAD * btnScale;
    float exitTopPad = EXIT_TOP_PAD * btnScale;
    float gap        = 18.0f * gs;                       // gap between visible buttons
    float imgW       = IMG_W_SRC * btnScale;
    float btnX       = (sw - imgW) / 2.0f;

    // Anchor first button content at 42% of screen height
    float firstContentY = sh * 0.42f;
    float y1 = firstContentY - topPad;         // top of new-game image
    float y2 = y1 + contentH + gap;            // top of load-game image
    float y3 = y2 + contentH + gap;            // top of exit image

    float btnNewGameX = btnX - (14.0f * gs);

    btnNewGame.load("assets/home_page/new-game 2.png", btnNewGameX, y1, btnScale);
    btnLoadGame.load("assets/home_page/load-game 2.png", btnX, y2, btnScale);
    btnExit.load   ("assets/home_page/exit 2.png",      btnX, y3, btnScale);

    // Set precise hit rects to the actual visible content areas (no whitespace)
    btnNewGame.setHitRect(btnNewGameX, y1 + topPad,     imgW, contentH);
    btnLoadGame.setHitRect(btnX, y2 + topPad,     imgW, contentH);
    btnExit.setHitRect   (btnX, y3 + exitTopPad, imgW, contentH);
}

void HomeScreen::unloadAssets() {
    UnloadTexture(bgTexture);
    UnloadTexture(titleTexture);
    btnNewGame.unload();
    btnLoadGame.unload();
    btnExit.unload();
}

void HomeScreen::update(float dt) {
    if (showLoadGameMsg) {
        msgTimer -= dt;
        if (msgTimer <= 0.0f || IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsKeyPressed(KEY_ESCAPE)) {
            showLoadGameMsg = false;
        }
        return;
    }

    if (btnNewGame.isClicked()) {
        nextScreen = AppScreen::NEW_GAME;
        shouldChangeScreen = true;
    } else if (btnLoadGame.isClicked()) {
        showLoadGameMsg = true;
        msgTimer = 2.0f;
    } else if (btnExit.isClicked()) {
        shouldQuit = true;
    }

    if (IsWindowResized()) {
        unloadAssets();
        loadAssets();
    }
}

void HomeScreen::draw() {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    float bgScale = std::max(sw / (float)bgTexture.width, sh / (float)bgTexture.height);
    float bgX = (sw - bgTexture.width * bgScale) / 2.0f;
    float bgY = (sh - bgTexture.height * bgScale) / 2.0f;
    DrawTextureEx(bgTexture, {bgX, bgY}, 0.0f, bgScale, WHITE);

    DrawTextureEx(titleTexture, {titleX, titleY}, 0.0f, titleScale, WHITE);

    btnNewGame.draw();
    btnLoadGame.draw();
    btnExit.draw();

    if (showLoadGameMsg) {
        const char* msg = "Load Game - Coming Soon!";
        int fontSize = static_cast<int>(32 * std::min(sw / 1280.0f, sh / 720.0f));
        int msgW = MeasureText(msg, fontSize);
        int boxW = msgW + 40;
        int boxH = fontSize + 30;
        int boxX = (sw - boxW) / 2;
        int boxY = (sh - boxH) / 2;
        DrawRectangle(boxX, boxY, boxW, boxH, Fade(BLACK, 0.8f));
        DrawRectangleLines(boxX, boxY, boxW, boxH, GOLD);
        DrawText(msg, boxX + 20, boxY + 15, fontSize, WHITE);
    }
}
