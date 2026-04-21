#include "include/models/tiles/ChanceTile.hpp"

ChanceTile::ChanceTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type)
    : CardTile(code, id, name, type) {}

void ChanceTile::onLanded(Player &player)
{
    CardTile::onLanded(player);
}
