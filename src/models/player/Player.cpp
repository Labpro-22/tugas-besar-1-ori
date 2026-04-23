#include "include/models/player/Player.hpp"

#include <algorithm>
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

        net_worth += property->getBuyPrice();

        if (property->getTileType() == "STREET")
        {
            int lvl = property->getLevel();
            if (lvl > 0)
            {
                int houses = (lvl >= 5) ? 4 : lvl;
                net_worth += static_cast<long long>(houses) * property->getHouseCost();
                if (lvl >= 5)
                {
                    net_worth += property->getHotelCost();
                }
            }
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
      shield_active(false),
      hand_cards() {}

std::string Player::getUsername() { return username; }
int Player::getCurrTile() { return curr_tile; }
int Player::getBalance() { return balance; }
int Player::getJailCounter() { return jail_counter; }
std::vector<PropertyTile *> Player::getOwnedProperties() { return owned_properties; }
std::string Player::getStatus() { return status; }
bool Player::isSkillUsed() { return skill_used; }
float Player::getDiscountActive() { return discount_active; }
bool Player::isShieldActive() { return shield_active; }
void Player::setCurrTile(int tile) { curr_tile = tile; }
void Player::addBalance(int amount) { balance += amount; }
void Player::setStatus(std::string newStatus) { status = newStatus; }
void Player::setSkillUsed(bool used) { skill_used = used; }
void Player::setDiscountActive(float discount) { discount_active = discount; }
void Player::setShieldActive(bool active) { shield_active = active; }

void Player::addOwnedProperty(PropertyTile *property)
{
    if (property == nullptr)
    {
        return;
    }

    for (PropertyTile *owned_property : owned_properties)
    {
        if (owned_property == property)
        {
            return;
        }
    }

    owned_properties.push_back(property);
    property->setOwner(this);
}

bool Player::removeOwnedProperty(std::string tile_code)
{
    auto iterator = std::find_if(
        owned_properties.begin(),
        owned_properties.end(),
        [&tile_code](PropertyTile *property)
        {
            return property != nullptr && property->getTileCode() == tile_code;
        });

    if (iterator == owned_properties.end())
    {
        return false;
    }

    if (*iterator != nullptr && (*iterator)->getTileOwner() == this)
    {
        (*iterator)->setOwner(nullptr);
    }

    owned_properties.erase(iterator);
    return true;
}

void Player::clearTurnModifiers()
{
    skill_used = false;
    discount_active = 0.0F;
    shield_active = false;
}

int Player::getNetWorth()
{
    return calculateNetWorth();
}

std::vector<SpecialPowerCard *> Player::getHandCards() const { return hand_cards; }
int Player::getHandSize() const { return static_cast<int>(hand_cards.size()); }
SpecialPowerCard *Player::getHandCard(int index) const {
    if (index < 0 || index >= static_cast<int>(hand_cards.size())) return nullptr;
    return hand_cards[index];
}

void Player::addHandCard(SpecialPowerCard *card) {
    if (card) hand_cards.push_back(card);
}

bool Player::removeHandCard(int index) {
    if (index < 0 || index >= static_cast<int>(hand_cards.size())) return false;
    hand_cards.erase(hand_cards.begin() + index);
    return true;
}
