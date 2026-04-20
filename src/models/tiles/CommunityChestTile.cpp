#include "include/models/tiles/CommunityChestTile.hpp"

CommunityChestTile::CommunityChestTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type)
    : CardTile(code, id, name, type) {}

void CommunityChestTile::onLanded(Player &player)
{
    CardTile::onLanded(player);
}
