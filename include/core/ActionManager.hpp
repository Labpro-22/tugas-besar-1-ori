#ifndef ACTIONMANAGER_HPP
#define ACTIONMANAGER_HPP

#include <vector>

class Card;
class Player;

class ActionManager
{
public:
    static void resolveCardEffect(
        Card &card,
        Player &active_player,
        const std::vector<Player *> &all_players);

private:
    static void applyHappyBirthday(
        Player &receiver,
        const std::vector<Player *> &all_players,
        int amount_per_player);

    static void applyLegislative(
        Player &payer,
        const std::vector<Player *> &all_players,
        int amount_per_player);

    static void transfer(Player &from, Player &to, int amount);
};

#endif
