#include "include/core/JailManager.hpp"

#include "include/models/player/Player.hpp"

bool JailManager::goToJail(Player &player, int jail_position)
{
    player.setCurrTile(jail_position);
    player.setStatus("JAIL");
    return true;
}