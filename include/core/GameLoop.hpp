#ifndef GAMELOOP_HPP
#define GAMELOOP_HPP

#include <string>
#include <tuple>
#include <vector>
#include "include/core/GameConfig.hpp"
#include "include/core/GameStates.hpp"

class GameState;
class Player;
class CommandHandler;
class BotController;
class LandingProcessor;
class BankruptcyProcessor;
class CardProcessor;
class RentCollector;

class GameLoop {
private:
    GameState *state;

    void initDecks();
    void distributeSkillCards();

    void start();
    void nextTurn();
    void checkWinCondition();

    static std::tuple<std::vector<Player*>, std::vector<Player*>, int>
        promptNewGame(int initBalance);
    static std::tuple<std::vector<Player*>, std::vector<Player*>, int>
        buildPlayersFromState(const GameStates::SaveState &sstate);
    void applyPropertyState(const GameStates::SaveState &sstate);
    GameStates::SaveState buildSaveState() const;

    friend class CommandHandler;
    friend class BotController;

public:
    GameLoop() : state(nullptr) {}
    ~GameLoop();

    static void run();

    // GUI factory: builds a ready-to-play GameLoop from GUI config
    static GameLoop* buildForGui(GameConfig& cfg, const std::string& configDir);

    // GUI helpers exposed for GameScreen
    void advanceTurn();
    void checkWinGui();
    GameState* getState() const;

    // Save / load for GUI
    void saveToFile(const std::string& filepath);
    static GameLoop* buildFromSave(const std::string& filepath, const std::string& configDir);
};

#endif