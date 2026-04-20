#ifndef PROPERTYMANAGER_HPP
#define PROPERTYMANAGER_HPP

#include "include/models/player/Player.hpp"
#include "include/models/tiles/PropertyTile.hpp"
#include "include/models/tiles/Tile.hpp"

class PropertyManager
{
public:
    static bool mortgage(Player &owner, PropertyTile &tile);
    static bool build(Player &owner, PropertyTile &tile);
    static bool compensate(Player &player, Tile &tile);
    static bool liquidate(Player &player, Tile &tile);
    static int calculateMaxLiquidation(Player &player);
};

#endif