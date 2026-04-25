#include "views/gui/GuiApp.hpp"
#include "views/gui/screens/HomeScreen.hpp"
#include "views/gui/screens/NewGameScreen.hpp"
#include "views/gui/screens/GameScreen.hpp"
#include "views/gui/screens/SettingsScreen.hpp"
#include "core/GameLoop.hpp"
#include "raylib.h"

GuiApp::GuiApp(int w, int h, const char* t)
    : screenWidth(w), screenHeight(h), title(t), activeScreen(AppScreen::HOME) {}

GameConfig& GuiApp::getGameConfig() { return gameConfig; }

GuiApp::~GuiApp() {
    if (currentScreen) currentScreen->unloadAssets();
    delete gameLoop;
    CloseWindow();
}

void GuiApp::switchScreen(AppScreen screen) {
    // capture load-save intent BEFORE unloading current screen
    bool   loadSave = currentScreen ? currentScreen->shouldLoadSave : false;
    std::string savePath = currentScreen ? currentScreen->loadSavePath : "";

    if (currentScreen) currentScreen->unloadAssets();

    // destroy previous game state when leaving GAME screen
    if (activeScreen == AppScreen::GAME || screen != AppScreen::GAME) {
        delete gameLoop;
        gameLoop = nullptr;
    }

    activeScreen = screen;
    switch (screen) {
        case AppScreen::HOME:
            currentScreen = std::make_unique<HomeScreen>();
            break;
        case AppScreen::NEW_GAME:
            currentScreen = std::make_unique<NewGameScreen>(gameConfig);
            break;
        case AppScreen::LOAD_GAME:
            currentScreen = std::make_unique<HomeScreen>();
            break;
        case AppScreen::GAME:
            if (loadSave) {
                try {
                    gameLoop = GameLoop::buildFromSave(savePath, "config/");
                } catch (...) {
                    gameLoop = GameLoop::buildForGui(gameConfig, "config/");
                }
            } else {
                gameLoop = GameLoop::buildForGui(gameConfig, "config/");
            }
            currentScreen = std::make_unique<GameScreen>(gameLoop);
            break;
        case AppScreen::SETTINGS:
            currentScreen = std::make_unique<SettingsScreen>();
            break;
    }
    currentScreen->loadAssets();
}

void GuiApp::run() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, title);
    SetTargetFPS(60);

    switchScreen(AppScreen::HOME);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        currentScreen->update(dt);

        if (currentScreen->shouldQuit) break;

        if (currentScreen->shouldChangeScreen) {
            currentScreen->shouldChangeScreen = false;
            switchScreen(currentScreen->nextScreen);
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        currentScreen->draw();
        EndDrawing();
    }
}
