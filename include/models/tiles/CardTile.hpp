#ifndef CARDTILE_HPP
#define CARDTILE_HPP

#include "include/models/tiles/ActionTile.hpp"

class CardTile : public ActionTile
{
public:
    CardTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type);
    void onLanded(Player &player) override;
};

#endif
