#ifndef BOARD_HPP
#define BOARD_HPP

#include <string>
#include <vector>

#include "include/models/tiles/PropertyTile.hpp"

class Board
{
private:
    std::vector<Tile *> tiles;

public:
    Board(const std::vector<Tile *> &tiles);
    Tile *getTileByCode(std::string code) const;
    Tile *getTileByIndex(int index) const;
    std::vector<PropertyTile *> getPropertiesByColorGroup(std::string color) const;
    int getColorGroupSize(std::string color) const;
    int getTileIndex(std::string code) const;
    int getTileCount() const;
};

#endif