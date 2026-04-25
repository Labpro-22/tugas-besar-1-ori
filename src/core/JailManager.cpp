#include "include/core/JailManager.hpp"

#include "include/models/player/Player.hpp"

bool JailManager::goToJail(Player &player, int jail_position)
{
    player.setCurrTile(jail_position);
    player.setStatus("JAIL");
    return true;
}

JailManager::RollResult JailManager::rollInJail(Player &player, Dice &dice, int currentJailTurns,
                                                 int boardSize, bool manual, int d1, int d2) {
    RollResult result;
    if (manual) dice.setDice(d1, d2);
    else dice.throwDice();

    result.isDouble = dice.isDouble();
    result.diceTotal = dice.getTotal();

    if (result.isDouble) {
        int cur = player.getCurrTile();
        player.setCurrTile((cur + dice.getTotal()) % boardSize);
        result.escaped = true;
        result.newJailTurns = 0;
    } else {
        result.escaped = false;
        result.newJailTurns = currentJailTurns + 1;
    }
    return result;
}