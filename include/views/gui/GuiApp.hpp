#ifndef GUIAPP_HPP
#define GUIAPP_HPP

#include <memory>
#include "views/gui/Screen.hpp"
#include "core/GameConfig.hpp"

class GameLoop;

class GuiApp {
private:
    int screenWidth;
    int screenHeight;
    const char* title;

    std::unique_ptr<Screen> currentScreen;
    AppScreen activeScreen;
    GameConfig gameConfig;
    GameLoop*  gameLoop = nullptr;

    void switchScreen(AppScreen screen);

public:
    GuiApp(int w = 1280, int h = 720, const char* t = "Nimonspoli");
    ~GuiApp();

    GameConfig& getGameConfig();
    void run();
};

#endif
