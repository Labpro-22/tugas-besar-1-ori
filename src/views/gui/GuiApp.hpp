#ifndef GUIAPP_HPP
#define GUIAPP_HPP

#include <memory>
#include "Screen.hpp"
#include "config/GameConfig.hpp"

class GuiApp {
private:
    int screenWidth;
    int screenHeight;
    const char* title;

    std::unique_ptr<Screen> currentScreen;
    AppScreen activeScreen;
    GameConfig gameConfig;

    void switchScreen(AppScreen screen);

public:
    GuiApp(int w = 1280, int h = 720, const char* t = "Nimonspoli");
    ~GuiApp();

    GameConfig& getGameConfig() { return gameConfig; }
    void run();
};

#endif
