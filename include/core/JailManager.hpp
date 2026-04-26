#ifndef JAILMANAGER_HPP
#define JAILMANAGER_HPP

#include "include/models/player/Player.hpp"
#include "include/models/dice/Dice.hpp"

class JailManager 
{
public:
    static bool goToJail(Player &player, int jail_position);
    struct RollResult {
        bool escaped = false;
        bool isDouble = false;
        int diceTotal = 0;
        int newJailTurns = 0;
    };
    static RollResult rollInJail(Player &player, Dice &dice, int currentJailTurns,
                                 int boardSize, bool manual, int d1, int d2);
};
#endif