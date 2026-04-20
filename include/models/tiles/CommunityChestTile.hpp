#ifndef COMMUNITYCHESTTILE_HPP
#define COMMUNITYCHESTTILE_HPP

#include "include/models/tiles/ActionTile.hpp"

class CommunityChestTile : public ActionTile
{
public:
    CommunityChestTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type);
    void onLanded(Player &player) override;
};

#endif