#ifndef CARDMANAGER_HPP
#define CARDMANAGER_HPP

#include "include/models/card/Card.hpp"
#include "include/models/player/Player.hpp"
#include <vector>

class CardManager
{
public:
    static void getCard(Player &player);
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