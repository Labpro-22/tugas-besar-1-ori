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
    public: 
        virtual void move();
        Player(std::string username);
        std::string getUsername();
        int getCurrTile();
        int getBalance();
        int getJailCounter();
        std::vector<PropertyTile*> getOwnedProperties();
        std::string getStatus();
        bool isSkillUsed();
        float getDiscountActive();
        bool isShieldActive();

        void setCurrTile(int tile);
        void addBalance(int amount);
        void setStatus(std::string status);
        void setSkillUsed(bool used);
        void setDiscountActive(float discount);
        void setShieldActive(bool active);
};

#endif