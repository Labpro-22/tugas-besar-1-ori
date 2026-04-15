#ifndef PLAYER_HPP
#define PLAYER_HPP

#include <string>
#include <vector>
#include "include/models/tiles/PropertyTile.hpp"
#include "include/models/dice/Dice.hpp"

class Player {
    protected:
        std::string username;
        int curr_tile;
        int balance;
        int jail_counter;
        std::vector<PropertyTile*> owned_properties;
        std::string status;
        bool skill_used;
        float discount_active;
        bool shield_active;
        int calculateNetWorth();
        Dice* dice;
    public: 
        virtual void move();
        Player(std::string username);
};

#endif