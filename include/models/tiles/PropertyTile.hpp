#ifndef PROPERTYTILE_HPP
#define PROPERTYTILE_HPP

#include "include/models/tiles/Tile.hpp"
#include <vector>

class Player;

class PropertyTile : public Tile
{
private:
    std::vector<int> rent_price;
    int mortgage_value;
    int level;
    Player *owner;
    bool mortgage_status;
    bool monopolized;
    std::string color_group;
    int buy_price;
    int tile_cost;
    int upgrade_house_cost;
    int upgrade_hotel_cost;
    int festival_multiplier;
    int festival_duration;

public:
    PropertyTile(
        const std::string &code, const std::string &id, const std::string &name, const std::string &type,
        std::vector<int> rent_price, int mortgage_value, int level,
        bool mortgage_status, std::string color_group, int buy_price, int tile_cost,
        int upgrade_house_cost, int upgrade_hotel_cost, int festival_multiplier, int festival_duration
    );
    
    void onLanded(Player &player, GameContext &ctx) override;
    int calculateRent() const;
    int applyFestival(int base_rent) const;
    bool isMortgage() const;
    bool isMonopolized() const;
    std::vector<int> getRentPrice() const;
    int getMortgageValue() const;
    int getLevel() const;
    std::string getColorGroup() const;
    int getBuyPrice() const;
    int getTileCost() const;
    int getHouseCost() const;
    int getHotelCost() const;
    int getFestivalMultiplier() const;
    int getFestivalDuration() const;
    Player *getTileOwner() const;

    void setOwner(Player *new_owner);
    void setMortgageStatus(bool is_mortgaged);
    void setMonopolized(bool is_monopolized);
    void setLevel(int new_level);
    void setFestivalState(int multiplier, int duration);
};

#endif