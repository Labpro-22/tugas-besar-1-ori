#ifndef COMMANDHANDLER_HPP
#define COMMANDHANDLER_HPP

#include <string>

class GameState;
class Player;
class LandingProcessor;
class BankruptcyProcessor;
class CardProcessor;
class RentCollector;
class GameLoop;

class CommandHandler {
private:
    GameState &state;
    LandingProcessor &landing;
    BankruptcyProcessor &bankruptcy;
    CardProcessor &cardProc;
    RentCollector &rentCollector;
    GameLoop &gameLoop;

    void processJailHuman(Player &p, bool manual, int d1, int d2,
                          bool &has_rolled, bool &turn_ended);
    void processNormalRollHuman(Player &p, bool manual, int d1, int d2,
                                bool &has_rolled, bool &turn_ended,
                                int &consec_doubles, bool &has_bonus_turn);
    void cmdBeli(Player &p);

public:
    CommandHandler(GameState &s, LandingProcessor &lp, BankruptcyProcessor &bp,
                   CardProcessor &cp, RentCollector &rc, GameLoop &gl);

    void executeTurn(Player &p);
};

#endif