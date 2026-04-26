#ifndef CARDPROCESSOR_HPP
#define CARDPROCESSOR_HPP

#include <string>
#include <vector>

class GameState;
class Player;
class PropertyTile;
class BankruptcyProcessor;
class LandingProcessor;

class CardProcessor {
private:
    GameState &state;
    BankruptcyProcessor &bankruptcy;
    LandingProcessor *landing;

public:
    CardProcessor(GameState &s, BankruptcyProcessor &bp);
    void setLandingProcessor(LandingProcessor *lp);

    void drawAndResolveChance(Player &p);
    void drawAndResolveCommunityChest(Player &p);
    void drawSkillCardAtTurnStart(Player &p);

    void cmdGunakanKemampuan(Player &p, int index);
    bool cmdFestival(Player &p, const std::string &code);
};

#endif