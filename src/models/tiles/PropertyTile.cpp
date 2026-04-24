#include "include/models/tiles/PropertyTile.hpp"
#include "include/core/GameContext.hpp"
#include "include/models/player/Player.hpp"

PropertyTile::PropertyTile(
    const std::string &code, 
    const std::string &id, 
    const std::string &name, 
    const std::string &type,
    std::vector<int> rent_price, 
    int mortgage_value, 
    int level,
    bool mortgage_status, 
    std::string color_group, 
    int buy_price, 
    int tile_cost,
    int upgrade_house_cost, 
    int upgrade_hotel_cost, 
    int festival_multiplier, 
    int festival_duration
) : Tile(code, id, name, type), 
    rent_price(rent_price), 
    mortgage_value(mortgage_value), 
    level(level),
    owner(nullptr), 
    mortgage_status(mortgage_status), 
    monopolized(false), 
    color_group(color_group),
    buy_price(buy_price), 
    tile_cost(tile_cost), 
    upgrade_house_cost(upgrade_house_cost),
    upgrade_hotel_cost(upgrade_hotel_cost), 
    festival_multiplier(festival_multiplier), 
    festival_duration(festival_duration) 
{
}

void PropertyTile::onLanded(Player &player, GameContext &ctx) 
{
    if (mortgage_status) 
    {
        ctx.displayMessage("Properti ini sedang digadaikan. Tidak ada sewa.");
        return;
    }

    ctx.handlePropertyLanding(player, *this);
}

int PropertyTile::calculateRent() const 
{
    if (level >= 0 && level < static_cast<int>(rent_price.size())) 
    {
        return rent_price[level];
    }

    return 0;
}

int PropertyTile::applyFestival(int base_rent) const 
{
    return base_rent * festival_multiplier;
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
    level = new_level; 
}

void PropertyTile::setFestivalState(int multiplier, int duration) 
{
    festival_multiplier = multiplier;
    festival_duration = duration;
}