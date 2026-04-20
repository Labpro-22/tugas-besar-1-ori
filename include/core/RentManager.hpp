#ifndef RENTMANAGER_HPP
#define RENTMANAGER_HPP

#include "include/models/player/Player.hpp"
#include "include/models/tiles/Tile.hpp"

class RentManager
{
public:
    static bool payRent(Player &buyer, Player &owner, Tile &tile);
};

#endif