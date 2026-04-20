#ifndef BOT_HPP
#define BOT_HPP

#include "include/models/player/Player.hpp"

class Bot : public Player
{
public:
    void move() override;
    Bot(std::string username);
};

#endif