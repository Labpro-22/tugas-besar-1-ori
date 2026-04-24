#ifndef GAMESTATE_HPP
#define GAMESTATE_HPP

#include <map>
#include <string>
#include <vector>

#include "include/core/GameConfig.hpp"
#include "include/core/EventManager.hpp"
#include "include/models/board/Board.hpp"
#include "include/models/card/CardDeck.hpp"
#include "include/models/card/ChanceCard.hpp"
#include "include/models/card/CommunityChest.hpp"
#include "include/models/dice/Dice.hpp"
#include "include/models/log-entry/LogEntry.hpp"
#include "include/models/player/Player.hpp"
#include "include/views/OutputFormatter.hpp"

class RentCollector;

class GameState {
public:
    std::vector<Tile*> tiles;
    Board board;
    std::vector<Player*> players;
    int active_player_id;
    int current_turn;
    int max_turn;
    std::vector<Player*> turn_order;
    int turn_order_idx;
    std::vector<LogEntry> transaction_log;

    Dice dice;
    GameConfig config;
    EventManager events;
    OutputFormatter formatter;
    RentCollector *rentCollector;

    CardDeck<ChanceCard> chance_deck;
    CardDeck<CommunityChestCard> community_deck;
    bool game_over;
    bool game_over_by_bankruptcy;

    std::map<Player*, int> jail_turns;

    std::vector<ChanceCard*> chance_cards;
    std::vector<CommunityChestCard*> community_cards;
    std::vector<SpecialPowerCard*> skill_cards;

    GameState(std::vector<Tile*> t, std::vector<Player*> p,
              std::vector<Player*> order, GameConfig cfg,
              int maxT, int currT, int activeId);
    ~GameState();

    int activePlayerCount() const;
    Player* getCreditorAt(int tileIdx) const;
    int getPlayerNum(Player &p) const;
    bool hasPendingAction(Player &p) const;
    void   addLog(Player &p, const std::string &action, const std::string &detail);
    Player* currentTurnPlayer() const;
    void recomputeMonopolyForGroup(const std::string &colorGroup);
};

#endif
