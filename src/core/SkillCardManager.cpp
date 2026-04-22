#include "include/core/SkillCardManager.hpp"

#include "include/models/player/Player.hpp"
#include "utils/exceptions/SkillCardException.hpp"

bool SkillCardManager::useSkillCard(Player &player)
{
    if (player.isSkillUsed())
    {
        throw LimitOnlyOneException();
    }

    player.setSkillUsed(true);
    return true;
}

void SkillCardManager::dropSkillCard(Player &player)
{
    player.clearTurnModifiers();
}