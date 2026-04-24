#include "include/models/tiles/JailTile.hpp"
#include "include/core/GameContext.hpp"

JailTile::JailTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type)
    : SpecialTile(code, id, name, type) {}

void JailTile::onLanded(Player &player, GameContext &ctx) {}
