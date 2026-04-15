#include "include/models/tiles/FreeParkingTile.hpp"

FreeParkingTile::FreeParkingTile(const std::string& code, const std::string& id, const std::string& name, const std::string& type) 
    : ActionTile(code, id, name, type) {}

// void FreeParkingTile::onLanded(Player& player);