#include "include/models/board/Board.hpp"
#include <algorithm>

Board::Board(const std::vector<Tile *> &tiles) : tiles(tiles) {}

Tile *Board::getTileByCode(std::string code) const 
{
    for (auto *t : tiles) 
    {
        if (t->getTileCode() == code) 
        {
            return t;
        }
    }
    return nullptr;
}

Tile *Board::getTileByIndex(int index) const 
{
    if (index >= 0 && index < static_cast<int>(tiles.size())) 
    {
        return tiles[index];
    }
    return nullptr;
}

std::vector<PropertyTile *> Board::getPropertiesByColorGroup(std::string color) const 
{
    std::vector<PropertyTile *> result;
    for (auto *t : tiles) 
    {
        auto *p = dynamic_cast<PropertyTile*>(t);
        if (p && p->getColorGroup() == color) 
        {
            result.push_back(p);
        }
    }
    return result;
}

int Board::getColorGroupSize(std::string color) const 
{
    return static_cast<int>(getPropertiesByColorGroup(color).size());
}

int Board::getTileIndex(std::string code) const 
{
    for (int i = 0; i < static_cast<int>(tiles.size()); i++)
    {
        if (tiles[i]->getTileCode() == code) 
        {
            return i;
        }
    }
    return -1;
}

int Board::getTileCount() const 
{ 
    return static_cast<int>(tiles.size()); 
}