#ifndef FESTIVALTILE_HPP
#define FESTIVALTILE_HPP

#include "include/models/tiles/ActionTile.hpp"

class FestivalTile : public ActionTile
{
public:
    FestivalTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type);
    void onLanded(Player &player, GameContext &ctx) override;
};

#endif