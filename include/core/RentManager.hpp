#ifndef RENTMANAGER_HPP
#define RENTMANAGER_HPP

#include "include/models/player/Player.hpp"
#include "include/models/tiles/Tile.hpp"
#include "include/models/tiles/PropertyTile.hpp"
#include "include/core/GameConfig.hpp"

#include <vector>

class RentManager {
public:
    static bool payRent(Player &buyer, Player &owner, Tile &tile);
    static int computeRent(PropertyTile &prop, int diceTotal, const GameConfig &cfg, const std::vector<Tile*> &allTiles);
    static int countRailroadsOwnedBy(Player *owner, const std::vector<Tile*> &allTiles);
    static int countUtilitiesOwnedBy(Player *owner, const std::vector<Tile*> &allTiles);
};
#endif