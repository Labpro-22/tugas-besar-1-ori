#ifndef FREEPARKINGTILE_HPP
#define FREEPARKINGTILE_HPP

#include "include/models/tiles/SpecialTile.hpp"

class FreeParkingTile : public SpecialTile
{
public:
    FreeParkingTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type);
    void onLanded(Player &player) override;
};

#endif