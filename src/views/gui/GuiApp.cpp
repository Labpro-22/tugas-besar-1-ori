#include "GuiApp.hpp"
#include "screens/HomeScreen.hpp"
#include "screens/NewGameScreen.hpp"
#include "screens/GameScreen.hpp"
#include "screens/SettingsScreen.hpp"
#include "raylib.h"

GuiApp::GuiApp(int w, int h, const char* t)
    : screenWidth(w), screenHeight(h), title(t), activeScreen(AppScreen::HOME) {}

GuiApp::~GuiApp() {
    if (currentScreen) {
        currentScreen->unloadAssets();
    }
    CloseWindow();
}

void GuiApp::switchScreen(AppScreen screen) {
    if (currentScreen) {
        currentScreen->unloadAssets();
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
            currentScreen = std::make_unique<HomeScreen>(); // Placeholder
            break;
        case AppScreen::GAME:
            currentScreen = std::make_unique<GameScreen>(gameConfig);
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

        if (currentScreen->shouldQuit) {
            break;
        }

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
