#include "include/models/tiles/JailTile.hpp"

JailTile::JailTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type)
    : ActionTile(code, id, name, type) {}

void JailTile::onLanded(Player &player)
{
    ActionTile::onLanded(player);
}