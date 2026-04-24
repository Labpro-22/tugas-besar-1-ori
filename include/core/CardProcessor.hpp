#ifndef CARDPROCESSOR_HPP
#define CARDPROCESSOR_HPP

#include <string>
#include <vector>

class GameState;
class Player;
class PropertyTile;
class BankruptcyProcessor;

class CardProcessor {
private:
    GameState &state;
    BankruptcyProcessor &bankruptcy;

public:
    CardProcessor(GameState &s, BankruptcyProcessor &bp);

    void drawAndResolveChance(Player &p);
    void drawAndResolveCommunityChest(Player &p);
    void drawSkillCardAtTurnStart(Player &p);

    void cmdGunakanKemampuan(Player &p, int index);
    void cmdFestival(Player &p, const std::string &code);
};

#endif