#ifndef TAXMANAGER_HPP
#define TAXMANAGER_HPP

#include "include/models/player/Player.hpp"
#include "include/core/GameConfig.hpp"

#include <string>

class TaxManager {
public:
    static bool payTax(Player &payer, int amount);
    static int calculatePPHFlat(const GameConfig &cfg);
    static int calculatePPHPct(Player &payer, const GameConfig &cfg);
    static int getPBMFlat(const GameConfig &cfg);
};

#endif