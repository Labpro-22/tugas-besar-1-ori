#ifndef SCREEN_HPP
#define SCREEN_HPP

#include <string>

enum class AppScreen { HOME, NEW_GAME, LOAD_GAME, GAME, SETTINGS };

class Screen {
public:
    virtual ~Screen() = default;
    virtual void loadAssets() = 0;
    virtual void unloadAssets() = 0;
    virtual void update(float dt) = 0;
    virtual void draw() = 0;

    AppScreen   nextScreen         = AppScreen::HOME;
    bool        shouldChangeScreen = false;
    bool        shouldQuit         = false;
    bool        shouldLoadSave     = false;  // true = load from savePath, not new game
    std::string loadSavePath;
};

#endif
