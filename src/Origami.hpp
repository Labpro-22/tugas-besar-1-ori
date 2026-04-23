#ifndef ORIGAMI_HPP
#define ORIGAMI_HPP

#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "../include/core/EventManager.hpp"
#include "../include/core/GameConfig.hpp"
#include "../include/core/GameStates.hpp"
#include "../include/models/board/Board.hpp"
#include "../include/models/card/CardDeck.hpp"
#include "../include/models/card/ChanceCard.hpp"
#include "../include/models/card/CommunityChest.hpp"
#include "../include/models/dice/Dice.hpp"
#include "../include/models/log-entry/LogEntry.hpp"
#include "../include/models/player/Player.hpp"
#include "views/OutputFormatter.hpp"

class Origami {
private:
    // ── core state ────────────────────────────────────────────────────────────
    std::vector<Tile*>    tiles;
    Board                 board;
    std::vector<Player*>  players;
    int                   active_player_id;
    int                   current_turn;
    int                   max_turn;
    std::vector<Player*>  turn_order;
    std::vector<LogEntry> transaction_log;

    // ── extended state ────────────────────────────────────────────────────────
    Dice                           dice;
    GameConfig                     config;
    EventManager                   events;
    OutputFormatter                formatter;
    CardDeck<ChanceCard>           chance_deck;
    CardDeck<CommunityChestCard>   community_deck;
    bool                           game_over;
    int                            turn_order_idx;
    std::map<Player*, int>         jail_turns;

    std::vector<ChanceCard*>         chance_cards;
    std::vector<CommunityChestCard*> community_cards;
    std::vector<SpecialPowerCard*>   skill_cards;

    // ── constructor / destructor ──────────────────────────────────────────────
    Origami(std::vector<Tile*> tiles, std::vector<Player*> players,
            std::vector<Player*> turnOrder, GameConfig cfg,
            int maxTurn, int currentTurn, int activeId);
    ~Origami();

    // ── setup ─────────────────────────────────────────────────────────────────
    static std::tuple<std::vector<Player*>, std::vector<Player*>, int>
        promptNewGame(int initBalance);
    static std::tuple<std::vector<Player*>, std::vector<Player*>, int>
        buildPlayersFromState(const GameStates::SaveState &state);
    void applyPropertyState(const GameStates::SaveState &state);
    void initDecks();

    // ── turn logic ────────────────────────────────────────────────────────────
    void start();
    void humanTurn(Player &p);
    void botTurn(Player &p);
    bool rollAndMove(Player &p, bool manual, int d1 = 0, int d2 = 0);
    void applyLanding(Player &p);
    void applyGoSalary(Player &p, int oldTile);

    // ── commands ──────────────────────────────────────────────────────────────
    void cmdCetakPapan();
    void cmdCetakAkta(const std::string &code);
    void cmdCetakProperti(Player &p);
    void cmdBeli(Player &p);
    void cmdBayarSewa(Player &p);
    void cmdBayarPajak(Player &p);
    void cmdGadai(Player &p, const std::string &code);
    void cmdTebus(Player &p, const std::string &code);
    void cmdBangun(Player &p, const std::string &code);
    void cmdLelang(Player &p, const std::string &code);
    void cmdFestival(Player &p, const std::string &code, int mult, int dur);
    void cmdBangkrut(Player &p);
    void cmdSimpan(const std::string &path, bool hasRolled);
    void cmdCetakLog();
    void cmdBayarDenda(Player &p);
    void cmdGunakanKemampuan(Player &p, int index);
    void cmdDropKemampuan(Player &p, int index);

    // ── helpers ───────────────────────────────────────────────────────────────
    GameStates::SaveState buildSaveState() const;
    int     activePlayerCount() const;
    Player* getCreditorAt(int tileIdx) const;
    void    addLog(Player &p, const std::string &action, const std::string &detail = "");
    bool    hasPendingAction(Player &p) const;
    int     getPlayerNum(Player &p) const;
    void    autoPayRent(Player &p, PropertyTile &prop);
    void    recoverForRent(Player &p, PropertyTile &prop);
    void    interactiveLelang(Player *seller, PropertyTile &prop, std::vector<Player*> &participants);
    bool    game_over_by_bankruptcy = false;

protected:
    void nextTurn();
    void checkWinCondition();
    void distributeSkillCards();

public:
    static void run();
};

#endif
