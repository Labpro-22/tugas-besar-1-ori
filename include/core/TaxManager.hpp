#ifndef TAXMANAGER_HPP
#define TAXMANAGER_HPP

#include "include/models/player/Player.hpp"

class TaxManager
{
public:
    static bool payTax(Player &payer, int amount);
};

#endif