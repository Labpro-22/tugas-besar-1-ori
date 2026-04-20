#include "include/models/tiles/CardTile.hpp"

CardTile::CardTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type)
    : ActionTile(code, id, name, type) {}

void CardTile::onLanded(Player &player)
{
    ActionTile::onLanded(player);
}
