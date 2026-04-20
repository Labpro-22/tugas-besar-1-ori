#ifndef JAILTILE_HPP
#define JAILTILE_HPP

#include "include/models/tiles/ActionTile.hpp"

class JailTile : public ActionTile
{
public:
    JailTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type);
    void onLanded(Player &player) override;
};

#endif