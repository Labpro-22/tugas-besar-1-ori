#include "include/core/FestivalManager.hpp"

#include "include/models/tiles/PropertyTile.hpp"

void FestivalManager::festival(PropertyTile &tile, int multiplier, int duration)
{
    tile.setFestivalState(multiplier, duration);
}