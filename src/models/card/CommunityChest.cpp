#include "include/models/card/CommunityChest.hpp"
#include "include/models/player/Player.hpp"

HappyBirthdayCard::HappyBirthdayCard() : CommunityChestCard() {}

void HappyBirthdayCard::action(Player &player)
{
    // Multi-player transfer ditangani oleh CardManager.
    (void)player;
}

SickCard::SickCard() : CommunityChestCard() {}

void SickCard::action(Player &player)
{
    player.addBalance(-700);
}

LegislativeCard::LegislativeCard() : CommunityChestCard() {}

void LegislativeCard::action(Player &player)
{
    // Multi-player transfer ditangani oleh CardManager.
    (void)player;
}