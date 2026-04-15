#ifndef BOT_HPP
#define BOT_HPP

#include "include/models/player/Player.hpp"

class Bot : public Player {
    private:
        void move() override;
    public: 
        Bot(std::string username);
};

#endif