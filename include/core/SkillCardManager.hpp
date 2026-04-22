#ifndef SKILLCARDMANAGER_HPP
#define SKILLCARDMANAGER_HPP

#include "include/models/player/Player.hpp"

class SkillCardManager
{
public:
    static bool useSkillCard(Player &player);
    static void dropSkillCard(Player &player);
};

#endif