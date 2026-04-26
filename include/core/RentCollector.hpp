#ifndef RENTCOLLECTOR_HPP
#define RENTCOLLECTOR_HPP

#include <string>

class GameState;
class Player;
class PropertyTile;
class BankruptcyProcessor;

class RentCollector {
private:
    GameState &state;
    BankruptcyProcessor &bankruptcy;

public:
    RentCollector(GameState &s, BankruptcyProcessor &bp);

    void autoPayRent(Player &p, PropertyTile &prop);
    void recoverForRent(Player &p, PropertyTile &prop);
    void recoverForDebt(Player &p, int amount, Player *creditor, const std::string &reason);
};

#endif
