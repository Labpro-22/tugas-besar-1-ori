#ifndef BANKRUPTCYPROCESSOR_HPP
#define BANKRUPTCYPROCESSOR_HPP

#include <string>
#include <vector>
#include <tuple>

class GameState;
class Player;
class PropertyTile;
class LandingProcessor;

class BankruptcyProcessor {
private:
    GameState &state;
    LandingProcessor &landing;

public:
    BankruptcyProcessor(GameState &s, LandingProcessor &lp);

    void processBankruptcy(Player &p, Player *creditor = nullptr);

    void cmdGadai(Player &p, const std::string &Code);
    void cmdTebus(Player &p, const std::string &Code);
    void cmdBangun(Player &p, const std::string &Code);
    void cmdLelang(Player &p, const std::string &Code);
    void handleAutoAuction(Player &p);
    void cmdBayarDenda(Player &p);
    bool tryForceJailFine(Player &p);
};

#endif