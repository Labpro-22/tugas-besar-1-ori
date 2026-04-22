#ifndef FESTIVALMANAGER_HPP
#define FESTIVALMANAGER_HPP

#include "include/models/tiles/PropertyTile.hpp"

class FestivalManager
{
public:
    static void festival(PropertyTile &tile, int multiplier, int duration);
};

#endif