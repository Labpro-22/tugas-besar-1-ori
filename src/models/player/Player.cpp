#include "include/models/player/Player.hpp"

void Player::move(){
    // TODO: implement
    // lagi pusing dikit, yg panggil gerak dari mana
}

int Player::calculateNetWorth(){
    // TODO: balance + iterasi property owned
}

Player::Player(std::string username) : username(username) {
    curr_tile = 0;
    balance = 6767; // TODO: set dari global config
    jail_counter = 0;
    status = "";
    skill_used = false;
    discount_active = 0.0;
    shield_active = false;;
}

std::string Player::getUsername() {return username;}
int Player::getCurrTile() {return curr_tile;}
int Player::getBalance() {return balance;}
int Player::getJailCounter() {return jail_counter;}
std::vector<PropertyTile*> Player::getOwnedProperties() {return owned_properties;}
std::string Player::getStatus() {return status;}
bool Player::isSkillUsed() {return skill_used;}
float Player::getDiscountActive() {return discount_active;}
bool Player::isShieldActive() {return shield_active;}

// Setters — dibutuhkan oleh card actions
void Player::setCurrTile(int tile) { curr_tile = tile; }
void Player::addBalance(int amount) { balance += amount; }
void Player::setStatus(std::string newStatus) { status = newStatus; }
void Player::setSkillUsed(bool used) { skill_used = used; }
void Player::setDiscountActive(float discount) { discount_active = discount; }
void Player::setShieldActive(bool active) { shield_active = active; }