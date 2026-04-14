#include "include/models/tiles/PropertyTile.hpp"

PropertyTile::PropertyTile(
            const std::string& code, const std::string& id, const std::string& name, const std::string& type,
            std::vector<int> rent_price, int mortgage_value, int level,
            bool mortgage_status, std::string color_group, int buy_price, int tile_cost, int upgrade_house_cost,
            int upgrade_hotel_cost, int festival_multiplier, int festival_duration) 
            : Tile(code, id, name, type), rent_price(rent_price), mortgage_value(mortgage_value),
            level(level), mortgage_status(mortgage_status), color_group(color_group), 
            buy_price(buy_price), tile_cost(tile_cost), upgrade_house_cost(upgrade_house_cost),
            upgrade_hotel_cost(upgrade_hotel_cost), festival_multiplier(festival_multiplier),
            festival_duration(festival_duration) {
                // nanti tambahin constructor buat Player* owner = null
            }

int PropertyTile::calculateRent() const {
    return 0;
}

bool PropertyTile::isMortgage() const {
    return mortgage_status;
}

// nanti dibenerin
bool PropertyTile::isMonopolized() const {
    return false;
}


std::vector<int> PropertyTile::getRentPrice() const {
    return rent_price;
}

int PropertyTile::getMortgageValue() const {
    return mortgage_value;
}

int PropertyTile::getLevel() const {
    return level;
}

std::string PropertyTile::getColorGroup() const {
    return color_group;
}

int PropertyTile::getBuyPrice() const {
    return buy_price;
}

int PropertyTile::getTileCost() const {
    return tile_cost;
}

int PropertyTile::getHouseCost() const {
    return upgrade_house_cost;
}

int PropertyTile::getHotelCost() const {
    return upgrade_hotel_cost;
}

int PropertyTile::getFestivalMultiplier() const {
    return festival_multiplier;
}

int PropertyTile::getFestivalDuration() const {
    return festival_duration;
}

// Player* PropertyTile::getTileOwner() const {
//     return owner;
// }