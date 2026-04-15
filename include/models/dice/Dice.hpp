#ifndef DICE_HPP
#define DICE_HPP

#include <random>
#include <chrono>

class Dice {
    protected:
        void throwDice();
        void setDice(int x, int y);
        bool isDouble();
        int getTotal();
    private:
        int die1;
        int die2;
        int double_count;
        std::mt19937 engine;
        std::uniform_int_distribution<int> dist;
    public: 
        Dice();
};

#endif