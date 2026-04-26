#include "include/core/SkillCardManager.hpp"
#include "include/utils/exceptions/SkillCardException.hpp"

bool SkillCardManager::useSkillCard(Player &player) {
    if (player.isSkillUsed()) {
        throw LimitOnlyOneException();
    }

    if (player.getHandSize() == 0) {
        return false;
    }

    player.setSkillUsed(true);
    return true;
}

void SkillCardManager::dropSkillCard(Player &player) {
    if (player.getHandSize() > 0) {
        player.removeHandCard(player.getHandSize() - 1);
    }
}

void SkillCardManager::applyTeleport(Player &player, int targetTile, int boardSize) {
    player.setCurrTile(targetTile);
}

void SkillCardManager::applyLasso(Player &user, Player &target, int targetTile) {
    target.setCurrTile(targetTile);
}

void SkillCardManager::applyDemolition(PropertyTile &target) {
    if (target.getLevel() > 0) {
        target.setLevel(target.getLevel() - 1);
    }
}

void SkillCardManager::applyGeneric(Player &player, SpecialPowerCard *card) {
    if (card) {
        card->action(player);
    }
}