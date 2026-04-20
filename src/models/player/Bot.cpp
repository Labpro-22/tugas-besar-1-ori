#include "include/models/player/Bot.hpp"

void Bot::move(){
    Player::move();
}

Bot::Bot(std::string username) : Player(username) {}
