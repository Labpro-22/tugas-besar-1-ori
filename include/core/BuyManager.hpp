#ifndef BUYMANAGER_HPP
#define BUYMANAGER_HPP

#include "include/models/player/Player.hpp"
#include "include/models/tiles/Tile.hpp"

class BuyManager
{
public:
    static bool buy(Player &buyer, Tile &tile);
};

#endif