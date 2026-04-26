#include "include/models/tiles/Tile.hpp"

Tile::Tile(const std::string &code, const std::string &id, const std::string &name, const std::string &type)
    : code(code), id(id), name(name), type(type) {}

std::string Tile::getTileCode() const
{
    return code;
}

std::string Tile::getTileID() const
{
    return id;
}

std::string Tile::getTileName() const
{
    return name;
}

std::string Tile::getTileType() const
{
    return type;
}

void Tile::onLanded(Player &player, GameContext &ctx) {}