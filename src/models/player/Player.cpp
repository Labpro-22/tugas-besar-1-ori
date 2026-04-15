#include "include/models/player/Player.hpp"

void Player::move(){
    // TODO: implement
    // lagi pusing dikit, yg panggil gerak dari mana
}

int Player::calculateNetWorth(){
    // TODO: balance + iterasi property owned
}

Player::Player(std::string username) : username(username) {
    curr_tile = 0; // start dari tile 0 ?
    balance = 6767; // berapa start balance ?
    jail_counter = 0;
    status = "";
    skill_used = false;
    discount_active = 0.0;
    shield_active = false;;
    dice = new Dice();
}
