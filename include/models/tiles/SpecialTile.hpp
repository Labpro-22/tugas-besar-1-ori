#ifndef SPECIALTILE_HPP
#define SPECIALTILE_HPP

#include "include/models/tiles/ActionTile.hpp"

class SpecialTile : public ActionTile
{
public:
    SpecialTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type);
    void onLanded(Player &player, GameContext &ctx) override;
};

#endif
