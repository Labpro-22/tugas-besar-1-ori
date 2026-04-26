#ifndef COMMUNITYCHESTTILE_HPP
#define COMMUNITYCHESTTILE_HPP

#include "include/models/tiles/CardTile.hpp"

class CommunityChestTile : public CardTile
{
public:
    CommunityChestTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type);
    void onLanded(Player &player, GameContext &ctx) override;
};

#endif