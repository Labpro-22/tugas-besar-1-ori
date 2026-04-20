#include "include/models/player/Player.hpp"

#include <climits>

void Player::move()
{
    if (jail_counter > 0)
    {
        jail_counter--;
        return;
    }

    Dice dice;
    dice.throwDice();
    curr_tile += dice.getTotal();
}

int Player::calculateNetWorth()
{
    long long net_worth = balance;

    for (PropertyTile *property : owned_properties)
    {
        if (property == nullptr)
        {
            continue;
        }

        if (property->isMortgage())
        {
            net_worth += property->getMortgageValue();
        }
        else
        {
            net_worth += property->getBuyPrice();
        }
    }

    if (net_worth > INT_MAX)
    {
        return INT_MAX;
    }

    if (net_worth < INT_MIN)
    {
        return INT_MIN;
    }

    return static_cast<int>(net_worth);
}

Player::Player(std::string username)
    : username(username),
      curr_tile(0),
      balance(0),
      jail_counter(0),
      owned_properties(),
      status("ACTIVE"),
      skill_used(false),
      discount_active(0.0F),
      shield_active(false) {}

std::string Player::getUsername() { return username; }
int Player::getCurrTile() { return curr_tile; }
int Player::getBalance() { return balance; }
int Player::getJailCounter() { return jail_counter; }
std::vector<PropertyTile *> Player::getOwnedProperties() { return owned_properties; }
std::string Player::getStatus() { return status; }
bool Player::isSkillUsed() { return skill_used; }
float Player::getDiscountActive() { return discount_active; }
bool Player::isShieldActive() { return shield_active; }