#ifndef PLAYER_HPP
#define PLAYER_HPP

#include <string>
#include <vector>
#include "include/models/tiles/PropertyTile.hpp"
#include "include/models/dice/Dice.hpp"
#include "include/models/card/SpecialPowerCard.hpp"

class Player
{
protected:
    std::string username;
    int curr_tile;
    int balance;
    int jail_counter;
    std::vector<PropertyTile *> owned_properties;
    std::string status;
    bool skill_used;
    float discount_active;
    bool shield_active;
    std::vector<SpecialPowerCard *> hand_cards;
    int calculateNetWorth() const;

public:
    virtual void move();
    virtual ~Player() = default;
    Player(std::string username);
    std::string getUsername();
    int getCurrTile();
    int getBalance();
    int getJailCounter();
    std::vector<PropertyTile *> getOwnedProperties();
    std::string getStatus();
    bool isSkillUsed();
    float getDiscountActive();
    bool isShieldActive();
    std::vector<SpecialPowerCard *> getHandCards() const;
    int getHandSize() const;
    SpecialPowerCard *getHandCard(int index) const;

    void setCurrTile(int tile);
    void addBalance(int amount);
    Player& operator+=(int amount);
    bool operator<(const Player& other) const;
    bool operator>(const Player& other) const;
    bool operator<=(const Player& other) const;
    bool operator>=(const Player& other) const;
    void setStatus(std::string status);
    void setSkillUsed(bool used);
    void setDiscountActive(float discount);
    void setShieldActive(bool active);

    void addHandCard(SpecialPowerCard *card);
    bool removeHandCard(int index);
    void clearHandCards();

    void addOwnedProperty(PropertyTile *property);
    bool removeOwnedProperty(std::string tile_code);
    void clearTurnModifiers();
    int getNetWorth() const;
};

#endif
