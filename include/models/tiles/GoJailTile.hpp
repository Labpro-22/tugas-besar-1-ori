#ifndef GOJAILTILE_HPP
#define GOJAILTILE_HPP

#include "include/models/tiles/ActionTile.hpp"

class GoJailTile : public ActionTile
{
public:
    GoJailTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type);
    void onLanded(Player &player) override;
};

#endif