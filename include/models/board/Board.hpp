#ifndef BOARD_HPP
#define BOARD_HPP

#include "include/models/tiles/PropertyTile.hpp"

class Board {
    private:
        std::vector<Tile*> Tiles;
    public: 
        Board(const std::vector<Tile*> Tiles);
        Tile* getTileByCode(std::string code);
        Tile* getTileByIndex(int index);
        std::vector<PropertyTile*> getPropertiesByColorGroup(std::string color);
        int getColorGroupSize(std::string color) const;
        int getTileIndex(std::string code) const;
};

#endif