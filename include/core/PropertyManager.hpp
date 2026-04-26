#ifndef PROPERTYMANAGER_HPP
#define PROPERTYMANAGER_HPP
#include "include/models/player/Player.hpp"
#include "include/models/tiles/PropertyTile.hpp"
#include "include/models/tiles/Tile.hpp"
#include "include/models/board/Board.hpp"
#include <vector>
#include <map>
#include <string>

class PropertyManager {
public:
    struct BuildOption {
        PropertyTile *prop = nullptr;
        int cost = 0;
        std::string levelStr;
    };
    static bool mortgage(Player &owner, PropertyTile &tile);
    static bool build(Player &owner, PropertyTile &tile);
    static bool compensate(Player &player, Tile &tile);
    static bool liquidate(Player &player, Tile &tile);
    static int calculateMaxLiquidation(Player &player);
    static std::map<std::string, std::vector<BuildOption>> getBuildableOptions(Player &owner, Board &board);
    static bool canBuildOn(PropertyTile *prop, Player &owner, Board &board);
    static std::vector<PropertyTile*> getGroupProperties(Board &board, const std::string &colorGroup);
    static int getMinLevelInGroup(Board &board, const std::string &colorGroup);
    static bool allReadyForHotel(Board &board, const std::string &colorGroup);
    static bool hasMonopoly(Player &owner, Board &board, const std::string &colorGroup);
    static void sellAllBuildingsInGroup(Player &owner, Board &board, const std::string &colorGroup,
                                        std::vector<std::tuple<std::string, int, int>> *outDetails = nullptr);
    static bool autoLiquidate(Player &p, Board &board, int targetAmount);
};
#endif