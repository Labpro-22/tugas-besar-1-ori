#ifndef GAMELOOP_HPP
#define GAMELOOP_HPP

#include <string>
#include <tuple>
#include <vector>
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
    static void run();
};

#endif