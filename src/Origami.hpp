#ifndef ORIGAMI_HPP
#define ORIGAMI_HPP

#include <string>
#include <tuple>
#include <vector>
#include "../include/models/board/Board.hpp"
#include "../include/models/player/Player.hpp"
#include "../include/models/log-entry/LogEntry.hpp"
#include "../include/core/GameStates.hpp"

using namespace std;

class Origami {
private:
    vector<Tile*> tiles;
    Board board;
    vector<Player*> players;
    int active_player_id;
    int current_turn;
    int max_turn;
    vector<Player*> turn_order;
    vector<LogEntry> transaction_log;

    Origami(vector<Tile*> tiles, vector<Player*> players,
            vector<Player*> turnOrder, int maxTurn, int currentTurn, int activeId);
    ~Origami();

    static tuple<vector<Player*>, vector<Player*>, int>
        promptNewGame(int initBalance);

    static tuple<vector<Player*>, vector<Player*>, int>
        buildPlayersFromState(const GameConfig::SaveState &state);

    void applyPropertyState(const GameConfig::SaveState &state);
    void start();

protected:
    void nextTurn();
    void checkWinCondition();
    void distributeSkillCards();

public:
    static void run();
};

#endif
