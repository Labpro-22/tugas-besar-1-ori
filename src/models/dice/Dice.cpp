#include "include/models/dice/Dice.hpp"

#include <stdexcept>

namespace
{
    bool isDiceValueValid(int value)
    {
        return value >= 1 && value <= 6;
    }
} // namespace

void Dice::throwDice()
{
    die1 = dist(engine);
    die2 = dist(engine);

    if (isDouble())
    {
        double_count++;
    }
    else
    {
        double_count = 0;
    }
}

void Dice::setDice(int x, int y)
{
    if (!isDiceValueValid(x) || !isDiceValueValid(y))
    {
        throw std::out_of_range("Dice values must be in range [1, 6].");
    }

    die1 = x;
    die2 = y;

    if (isDouble())
    {
        double_count++;
    }
    else
    {
        double_count = 0;
    }
}

bool Dice::isDouble() const
{
    return die1 == die2;
}

int Dice::getTotal() const
{
    return die1 + die2;
}

int Dice::getDoubleCount() const
{
    return double_count;
}

int Dice::getDie1() const
{
    return die1;
}

int Dice::getDie2() const
{
    return die2;
}

Dice::Dice() : die1(0), die2(0), double_count(0),
               engine(std::chrono::high_resolution_clock::now().time_since_epoch().count()),
               dist(1, 6) {}