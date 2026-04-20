#include "include/models/tiles/TaxTile.hpp"

TaxTile::TaxTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type)
    : ActionTile(code, id, name, type) {}

void TaxTile::onLanded(Player &player)
{
    ActionTile::onLanded(player);
}