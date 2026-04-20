#ifndef CHANCETILE_HPP
#define CHANCETILE_HPP

#include "include/models/tiles/CardTile.hpp"

class ChanceTile : public CardTile
{
public:
    ChanceTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type);
    void onLanded(Player &player) override;
};

#endif