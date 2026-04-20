#ifndef STARTTILE_HPP
#define STARTTILE_HPP

#include "include/models/tiles/ActionTile.hpp"

class StartTile : public ActionTile
{
public:
    StartTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type);
    void onLanded(Player &player) override;
};

#endif