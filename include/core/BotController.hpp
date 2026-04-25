#ifndef BOTCONTROLLER_HPP
#define BOTCONTROLLER_HPP

class GameState;
class Player;
class LandingProcessor;
class BankruptcyProcessor;
class CardProcessor;
class RentCollector;

class BotController {
private:
    GameState &state;
    LandingProcessor &landing;
    BankruptcyProcessor &bankruptcy;
    CardProcessor &cardProc;
    RentCollector &rentCollector;

public:
    BotController(GameState &s, LandingProcessor &lp, BankruptcyProcessor &bp,
                  CardProcessor &cp, RentCollector &rc);
    void executeTurn(Player &p, int consecDoubles = 0);
};

#endif