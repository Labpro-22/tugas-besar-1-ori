#ifndef PROPERTYTILE_HPP
#define PROPERTYTILE_HPP

#include "include/models/tiles/Tile.hpp"

class PropertyTile : public Tile {
    private:
        std::vector<int> rent_price;
        int mortgage_value;
        int level;
        // std::Player* owner;
        bool mortgage_status;
        std::string color_group;
        int buy_price;
        int tile_cost;
        int upgrade_house_cost;
        int upgrade_hotel_cost;
        int festival_multiplier;
        int festival_duration;
    
    public:

        PropertyTile(
            const std::string& code, const std::string& id, const std::string& name, const std::string& type,
            std::vector<int> rent_price, int mortgage_value, int level,
            bool mortgage_status, std::string color_group, int buy_price, int tile_cost, int upgrade_house_cost,
            int upgrade_hotel_cost, int festival_multiplier, int festival_duration);

        int calculateRent() const;
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
        // Player* getTileOwner() const;
};

#endif