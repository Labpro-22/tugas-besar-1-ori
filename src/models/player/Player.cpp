#include "include/models/player/Player.hpp"
#include <algorithm>

Player::Player(std::string username) : 
    username(username), 
    curr_tile(0), 
    balance(0),
    status("ACTIVE"), 
    skill_used(false), 
    discount_active(0.0f), 
    shield_active(false) 
{
}

void Player::move() {}

std::string Player::getUsername() 
{ 
    return username; 
}

int Player::getCurrTile() 
{ 
    return curr_tile; 
}

int Player::getBalance() 
{ 
    return balance; 
}

std::vector<PropertyTile *> Player::getOwnedProperties() 
{ 
    return owned_properties; 
}

std::string Player::getStatus() 
{ 
    return status; 
}

bool Player::isSkillUsed() 
{ 
    return skill_used; 
}

float Player::getDiscountActive() 
{ 
    return discount_active; 
}

bool Player::isShieldActive() 
{ 
    return shield_active; 
}

std::vector<SpecialPowerCard *> Player::getHandCards() const 
{ 
    return hand_cards; 
}

int Player::getHandSize() const 
{ 
    return static_cast<int>(hand_cards.size()); 
}

SpecialPowerCard *Player::getHandCard(int index) const 
{
    if (index >= 0 && index < static_cast<int>(hand_cards.size())) 
    {
        return hand_cards[index];
    }
    return nullptr;
}

void Player::setCurrTile(int tile) 
{ 
    curr_tile = tile; 
}

Player& Player::operator+=(int amount) 
{
    balance += amount;
    return *this;
}

bool Player::operator<(const Player& other) const 
{ 
    return getNetWorth() < other.getNetWorth(); 
}

bool Player::operator>(const Player& other) const 
{ 
    return getNetWorth() > other.getNetWorth(); 
}

bool Player::operator<=(const Player& other) const 
{ 
    return getNetWorth() <= other.getNetWorth(); 
}

bool Player::operator>=(const Player& other) const 
{ 
    return getNetWorth() >= other.getNetWorth(); 
}

void Player::setStatus(std::string status) 
{ 
    this->status = status; 
}

void Player::setSkillUsed(bool used) 
{ 
    skill_used = used; 
}

void Player::setDiscountActive(float discount) 
{ 
    discount_active = discount; 
}

void Player::setShieldActive(bool active) 
{ 
    shield_active = active; 
}

void Player::addHandCard(SpecialPowerCard *card) 
{
    if (card) 
    {
        hand_cards.push_back(card);
    }
}

bool Player::removeHandCard(int index) 
{
    if (index >= 0 && index < static_cast<int>(hand_cards.size())) 
    {
        hand_cards.erase(hand_cards.begin() + index);
        return true;
    }
    return false;
}

void Player::clearHandCards() 
{ 
    hand_cards.clear(); 
}

void Player::addOwnedProperty(PropertyTile *property) 
{
    if (property) 
    {
        property->setOwner(this);
        owned_properties.push_back(property);
    }
}

bool Player::removeOwnedProperty(std::string tile_code) 
{
    for (int i = 0; i < static_cast<int>(owned_properties.size()); i++) 
    {
        if (owned_properties[i] && owned_properties[i]->getTileCode() == tile_code) 
        {
            owned_properties[i]->setOwner(nullptr);
            owned_properties.erase(owned_properties.begin() + i);
            return true;
        }
    }
    return false;
}

void Player::clearTurnModifiers() 
{
    skill_used = false;
    discount_active = 0.0f;
    shield_active = false;
}

int Player::calculateNetWorth() const
{
    // Spec: total kekayaan = uang tunai + harga beli seluruh properti
    // (termasuk yang digadaikan) + harga beli seluruh bangunan yang telah
    // didirikan (harga penuh, bukan separuh).
    int total = balance;

    for (auto *p : owned_properties)
    {
        if (!p) continue;
        total += p->getBuyPrice();
        if (p->getTileType() == "STREET" && p->getLevel() > 0)
        {
            int bldCost = (p->getLevel() >= 5)
                ? p->getHotelCost()
                : p->getHouseCost() * p->getLevel();
            total += bldCost;
        }
    }

    return total;
}

int Player::getNetWorth() const 
{ 
    return calculateNetWorth(); 
}