#ifndef CHANCETILE_HPP
#define CHANCETILE_HPP

#include "include/models/tiles/ActionTile.hpp"

class ChanceTile : public ActionTile {
    public:
        ChanceTile(const std::string& code, const std::string& id, const std::string& name, const std::string& type);
        // void onLanded(Player& player);
};

#endif