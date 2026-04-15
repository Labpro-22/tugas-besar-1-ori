#include "include/models/tiles/GoJailTile.hpp"

GoJailTile::GoJailTile(const std::string& code, const std::string& id, const std::string& name, const std::string& type) 
    : ActionTile(code, id, name, type) {}

// void GoJailTile::onLanded(Player& player);