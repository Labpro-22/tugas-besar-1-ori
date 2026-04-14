#include "include/models/tiles/FestivalTile.hpp"

FestivalTile::FestivalTile(const std::string& code, const std::string& id, const std::string& name, const std::string& type) 
    : ActionTile(code, id, name, type) {}

// void FestivalTile::onLanded(Player& player);