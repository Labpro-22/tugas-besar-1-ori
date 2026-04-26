#ifndef DICE_HPP
#define DICE_HPP

#include <random>
#include <chrono>

class Dice
{
private:
    int die1;
    int die2;
    int double_count;
    std::mt19937 engine;
    std::uniform_int_distribution<int> dist;

public:
    Dice();
    void throwDice();
    void setDice(int x, int y);
    bool isDouble() const;
    int getTotal() const;
    int getDoubleCount() const;
    int getDie1() const;
    int getDie2() const;
};

#endif