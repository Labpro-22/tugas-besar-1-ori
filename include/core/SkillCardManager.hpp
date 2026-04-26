#ifndef SKILLCARDMANAGER_HPP
#define SKILLCARDMANAGER_HPP

#include "include/models/player/Player.hpp"
#include "include/models/tiles/PropertyTile.hpp"
#include "include/models/card/SpecialPowerCard.hpp"

class SkillCardManager
{
public:
    static bool useSkillCard(Player &player);
    static void dropSkillCard(Player &player);
    static void applyTeleport(Player &player, int targetTile, int boardSize);
    static void applyLasso(Player &user, Player &target, int targetTile);
    static void applyDemolition(PropertyTile &target);
    static void applyGeneric(Player &player, SpecialPowerCard *card);
};

#endif