#ifndef GOJAILTILE_HPP
#define GOJAILTILE_HPP

#include "include/models/tiles/SpecialTile.hpp"

class GoJailTile : public SpecialTile
{
public:
    GoJailTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type);
    void onLanded(Player &player, GameContext &ctx) override;
};

#endif