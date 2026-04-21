#include "include/models/tiles/PropertyTile.hpp"

#include <algorithm>

PropertyTile::PropertyTile(
    const std::string &code, const std::string &id, const std::string &name, const std::string &type,
    std::vector<int> rent_price, int mortgage_value, int level,
    bool mortgage_status, std::string color_group, int buy_price, int tile_cost, int upgrade_house_cost,
    int upgrade_hotel_cost, int festival_multiplier, int festival_duration)
    : Tile(code, id, name, type), rent_price(rent_price), mortgage_value(mortgage_value),
      level(level), owner(nullptr), mortgage_status(mortgage_status), monopolized(false), color_group(color_group),
      buy_price(buy_price), tile_cost(tile_cost), upgrade_house_cost(upgrade_house_cost),
      upgrade_hotel_cost(upgrade_hotel_cost), festival_multiplier(std::max(1, festival_multiplier)),
      festival_duration(std::max(0, festival_duration)) {}

int PropertyTile::calculateRent() const
{
    if (rent_price.empty())
    {
        return 0;
    }

    int bounded_level = std::max(0, level);
    if (bounded_level >= static_cast<int>(rent_price.size()))
    {
        bounded_level = static_cast<int>(rent_price.size()) - 1;
    }

    int rent_value = rent_price[bounded_level];
    if (festival_duration > 0)
    {
        rent_value *= std::max(1, festival_multiplier);
    }

    return rent_value;
}

bool PropertyTile::isMortgage() const
{
    return mortgage_status;
}

bool PropertyTile::isMonopolized() const
{
    return monopolized;
}

std::vector<int> PropertyTile::getRentPrice() const
{
    return rent_price;
}

int PropertyTile::getMortgageValue() const
{
    return mortgage_value;
}

int PropertyTile::getLevel() const
{
    return level;
}

std::string PropertyTile::getColorGroup() const
{
    return color_group;
}

int PropertyTile::getBuyPrice() const
{
    return buy_price;
}

int PropertyTile::getTileCost() const
{
    return tile_cost;
}

int PropertyTile::getHouseCost() const
{
    return upgrade_house_cost;
}

int PropertyTile::getHotelCost() const
{
    return upgrade_hotel_cost;
}

int PropertyTile::getFestivalMultiplier() const
{
    return festival_multiplier;
}

int PropertyTile::getFestivalDuration() const
{
    return festival_duration;
}

Player *PropertyTile::getTileOwner() const
{
    return owner;
}

void PropertyTile::setOwner(Player *new_owner)
{
    owner = new_owner;
}

void PropertyTile::setMortgageStatus(bool is_mortgaged)
{
    mortgage_status = is_mortgaged;
}

void PropertyTile::setMonopolized(bool is_monopolized)
{
    monopolized = is_monopolized;
}

void PropertyTile::setLevel(int new_level)
{
    if (new_level < 0)
    {
        level = 0;
        return;
    }

    level = new_level;
}

void PropertyTile::setFestivalState(int multiplier, int duration)
{
    festival_multiplier = std::max(1, multiplier);
    festival_duration = std::max(0, duration);
}