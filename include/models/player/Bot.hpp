#ifndef BOT_HPP
#define BOT_HPP

#include "include/models/player/Player.hpp"

class Bot : public Player
{
public:
    void move() override;
    Bot(std::string username);
    static PropertyTile* chooseFestivalProperty(const std::vector<PropertyTile*> &props);
    static bool shouldBuyStreet(int balance, int price);
};

#endif