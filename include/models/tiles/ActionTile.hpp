#ifndef ACTIONTILE_HPP
#define ACTIONTILE_HPP

#include "include/models/tiles/Tile.hpp"

class ActionTile : public Tile
{
public:
    ActionTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type);
    void onLanded(Player &player) override;
};

#endif