#include "include/models/board/Board.hpp"

namespace
{
    int normalizeIndex(int index, int size)
    {
        int normalized_index = index % size;
        if (normalized_index < 0)
        {
            normalized_index += size;
        }
        return normalized_index;
    }
} // namespace

Board::Board(const std::vector<Tile *> tiles) : tiles(tiles) {}

Tile *Board::getTileByCode(std::string code)
{
    for (Tile *tile : tiles)
    {
        if (tile != nullptr && tile->getTileCode() == code)
        {
            return tile;
        }
    }

    return nullptr;
}

Tile *Board::getTileByIndex(int index)
{
    if (tiles.empty())
    {
        return nullptr;
    }

    int normalized_index = normalizeIndex(index, static_cast<int>(tiles.size()));
    return tiles[normalized_index];
}

std::vector<PropertyTile *> Board::getPropertiesByColorGroup(std::string color)
{
    std::vector<PropertyTile *> matching_properties;

    for (Tile *tile : tiles)
    {
        PropertyTile *property_tile = dynamic_cast<PropertyTile *>(tile);
        if (property_tile != nullptr && property_tile->getColorGroup() == color)
        {
            matching_properties.push_back(property_tile);
        }
    }

    return matching_properties;
}

int Board::getColorGroupSize(std::string color) const
{
    int color_group_size = 0;

    for (Tile *tile : tiles)
    {
        const PropertyTile *property_tile = dynamic_cast<PropertyTile *>(tile);
        if (property_tile != nullptr && property_tile->getColorGroup() == color)
        {
            color_group_size++;
        }
    }

    return color_group_size;
}

int Board::getTileIndex(std::string code) const
{
    for (std::size_t index = 0; index < tiles.size(); ++index)
    {
        if (tiles[index] != nullptr && tiles[index]->getTileCode() == code)
        {
            return static_cast<int>(index);
        }
    }

    return -1;
}