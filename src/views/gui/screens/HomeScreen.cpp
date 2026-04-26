#include "views/gui/screens/HomeScreen.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>

HomeScreen::HomeScreen() : bgTexture{}, titleTexture{}, titleScale(0.5f), titleX(0), titleY(0),
    showPopup(false), globalScale(1.0f) {}

void HomeScreen::loadAssets() {
    bgTexture    = LoadTexture("assets/page_background.png");
    titleTexture = LoadTexture("assets/home_page/title.png");

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    globalScale = std::min(sw / 1280.0f, sh / 720.0f);

    titleScale = std::min(sw / (float)titleTexture.width * 0.7f,
                          sh / (float)titleTexture.height * 0.3f);
    titleX = (sw - titleTexture.width * titleScale) / 2.0f;
    titleY = sh * 0.12f;

    const float TARGET_H      = 60.0f;
    const float IMG_CONTENT_H = 78.0f;
    const float IMG_TOP_PAD   = 298.0f;
    const float EXIT_TOP_PAD  = 300.0f;
    const float IMG_W_SRC     = 707.0f;

    float btnScale  = TARGET_H * globalScale / IMG_CONTENT_H;
    float contentH  = IMG_CONTENT_H * btnScale;
    float topPad    = IMG_TOP_PAD    * btnScale;
    float exitTopPad= EXIT_TOP_PAD   * btnScale;
    float gap       = 18.0f * globalScale;
    float imgW      = IMG_W_SRC * btnScale;
    float btnX      = (sw - imgW) / 2.0f;

    float firstContentY = sh * 0.42f;
    float y1 = firstContentY - topPad;
    float y2 = y1 + contentH + gap;
    float y3 = y2 + contentH + gap;

    float btnNewGameX = btnX - (14.0f * globalScale);

    btnNewGame.load("assets/home_page/new-game 2.png",  btnNewGameX, y1, btnScale);
    btnLoadGame.load("assets/home_page/load-game 2.png", btnX,        y2, btnScale);
    btnExit.load    ("assets/home_page/exit 2.png",      btnX,        y3, btnScale);

    // set tight hit rects so overlapping images don't steal clicks
    btnNewGame.setHitRect(btnNewGameX, y1 + topPad,      imgW, contentH);
    btnLoadGame.setHitRect(btnX,       y2 + topPad,      imgW, contentH);
    btnExit.setHitRect    (btnX,       y3 + exitTopPad,  imgW, contentH);
}

void HomeScreen::unloadAssets() {
    UnloadTexture(bgTexture);
    UnloadTexture(titleTexture);
    btnNewGame.unload();
    btnLoadGame.unload();
    btnExit.unload();
}

void HomeScreen::update(float dt) {
    (void)dt;

    // popup blocks everything else
    if (showPopup) {
        // text input for load path
        int key = GetCharPressed();
        while (key > 0) {
            if (loadPathLen < 126 && key >= 32 && key <= 126) {
                loadPathBuf[loadPathLen++] = (char)key;
                loadPathBuf[loadPathLen]   = '\0';
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && loadPathLen > 0) {
            loadPathBuf[--loadPathLen] = '\0';
            loadError.clear();
        }

        if (IsKeyPressed(KEY_ENTER) && loadPathLen > 0) {
            std::string path(loadPathBuf);
            // auto-prefix "save/" if no directory separator
            if (path.find('/') == std::string::npos && path.find('\\') == std::string::npos)
                path = "save/" + path;
            // validate file exists
            std::ifstream test(path);
            if (test.good()) {
                shouldLoadSave = true;
                loadSavePath   = path;
                nextScreen     = AppScreen::GAME;
                shouldChangeScreen = true;
                showPopup = false;
                loadError.clear();
            } else {
                loadError = "File tidak ditemukan: " + path;
            }
            return;
        }
        return;
    }

    if (btnNewGame.isClicked()) {
        nextScreen = AppScreen::NEW_GAME;
        shouldChangeScreen = true;
    } else if (btnLoadGame.isClicked()) {
        showPopup = true;
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

    if (showPopup) {
        DrawRectangle(0, 0, sw, sh, {0, 0, 0, 160});

        float popW = 420.0f * globalScale, popH = 210.0f * globalScale;
        float popX = (sw - popW) / 2.0f,  popY = (sh - popH) / 2.0f;

        DrawRectangleRounded({popX, popY, popW, popH}, 0.15f, 8, {255, 243, 210, 255});
        DrawRectangleRoundedLines({popX, popY, popW, popH}, 0.15f, 8, {80, 40, 35, 255});

        int titleSz = (int)(20 * globalScale);
        const char* title = "LOAD GAME";
        int tw = MeasureText(title, titleSz);
        DrawText(title, (int)(popX + (popW - tw) / 2.0f),
                 (int)(popY + 18.0f * globalScale), titleSz, {80, 40, 35, 255});

        int subSz = (int)(12 * globalScale);
        DrawText("Nama file save (contoh: test.txt):",
                 (int)(popX + 18.0f * globalScale),
                 (int)(popY + 18.0f * globalScale + titleSz + 12.0f * globalScale),
                 subSz, BLACK);

        // text input field
        float inY  = popY + 18.0f * globalScale + titleSz + 12.0f * globalScale + subSz + 8.0f * globalScale;
        float inW  = popW - 36.0f * globalScale;
        float inH  = 28.0f * globalScale;
        float inX  = popX + 18.0f * globalScale;
        DrawRectangleRec({inX, inY, inW, inH}, WHITE);
        DrawRectangleLinesEx({inX, inY, inW, inH}, 1.5f, {80, 40, 35, 255});
        std::string inp(loadPathBuf);
        if ((int)GetTime() % 2 == 0) inp += "|";
        DrawText(inp.c_str(), (int)(inX + 6), (int)(inY + (inH - subSz) / 2.0f), subSz, BLACK);

        // error message
        if (!loadError.empty()) {
            int errSz = (int)(11 * globalScale);
            int errW = MeasureText(loadError.c_str(), errSz);
            DrawText(loadError.c_str(), (int)(popX + (popW - errW) / 2.0f),
                     (int)(inY + inH + 4.0f * globalScale), errSz, RED);
        }

        // BATAL button
        float batalW = 80.0f * globalScale, batalH = 26.0f * globalScale;
        float batalX = popX + (popW - batalW) / 2.0f;
        float batalY = popY + popH - batalH - 10.0f * globalScale;
        bool hBatal = (GetMouseX() >= batalX && GetMouseX() <= batalX + batalW &&
                       GetMouseY() >= batalY && GetMouseY() <= batalY + batalH);
        DrawRectangleRec({batalX, batalY, batalW, batalH},
                         hBatal ? Color{100,50,45,255} : Color{80,40,35,255});
        int bSz = (int)(12 * globalScale);
        int bw  = MeasureText("BATAL", bSz);
        DrawText("BATAL", (int)(batalX + (batalW - bw)/2.0f),
                 (int)(batalY + (batalH - bSz)/2.0f), bSz, WHITE);
        if (hBatal && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            showPopup = false;
            loadPathLen = 0; loadPathBuf[0] = '\0';
            loadError.clear();
        }

        int hintSz = (int)(10 * globalScale);
        const char* hint = "ENTER untuk load";
        int hw = MeasureText(hint, hintSz);
        DrawText(hint, (int)(popX + (popW - hw) / 2.0f),
                 (int)(batalY - 14.0f * globalScale), hintSz, {120, 80, 75, 255});
    }
}
