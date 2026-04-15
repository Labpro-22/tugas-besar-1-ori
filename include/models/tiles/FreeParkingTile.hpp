#ifndef FREEPARKINGTILE_HPP
#define FREEPARKINGTILE_HPP

#include "include/models/tiles/ActionTile.hpp"

class FreeParkingTile : public ActionTile {
    public:
        FreeParkingTile(const std::string& code, const std::string& id, const std::string& name, const std::string& type);
        // void onLanded(Player& player) override;
};

#endif