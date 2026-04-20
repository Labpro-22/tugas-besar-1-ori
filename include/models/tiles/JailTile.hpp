#ifndef JAILTILE_HPP
#define JAILTILE_HPP

#include "include/models/tiles/SpecialTile.hpp"

class JailTile : public SpecialTile
{
public:
    JailTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type);
    void onLanded(Player &player) override;
};

#endif