#include "include/core/ActionManager.hpp"

#include "include/models/card/Card.hpp"
#include "include/models/card/CommunityChest.hpp"
#include "include/models/player/Player.hpp"

namespace
{
    bool isValidCounterparty(const Player *player, const Player &active_player)
    {
        return player != nullptr && player != &active_player;
    }
} // namespace

void ActionManager::resolveCardEffect(
    Card &card,
    Player &active_player,
    const std::vector<Player *> &all_players)
{
    if (dynamic_cast<HappyBirthdayCard *>(&card) != nullptr)
    {
        applyHappyBirthday(active_player, all_players, 100);
        return;
    }

    if (dynamic_cast<LegislativeCard *>(&card) != nullptr)
    {
        applyLegislative(active_player, all_players, 200);
        return;
    }

    card.action(active_player);
}

void ActionManager::applyHappyBirthday(
    Player &receiver,
    const std::vector<Player *> &all_players,
    int amount_per_player)
{
    for (Player *player : all_players)
    {
        if (!isValidCounterparty(player, receiver))
        {
            continue;
        }

        transfer(*player, receiver, amount_per_player);
    }
}

void ActionManager::applyLegislative(
    Player &payer,
    const std::vector<Player *> &all_players,
    int amount_per_player)
{
    for (Player *player : all_players)
    {
        if (!isValidCounterparty(player, payer))
        {
            continue;
        }

        transfer(payer, *player, amount_per_player);
    }
}

void ActionManager::transfer(Player &from, Player &to, int amount)
{
    if (amount <= 0 || &from == &to)
    {
        return;
    }

    from.addBalance(-amount);
    to.addBalance(amount);
}
