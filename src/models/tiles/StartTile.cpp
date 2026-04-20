#include "include/models/tiles/StartTile.hpp"

StartTile::StartTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type)
    : ActionTile(code, id, name, type) {}

void StartTile::onLanded(Player &player)
{
    ActionTile::onLanded(player);
}