#include "include/core/TaxManager.hpp"

bool TaxManager::payTax(Player &payer, int amount) {
    if (payer.getBalance() < amount) {
        return false;
    }
    
    payer += -amount;
    return true;
}

int TaxManager::calculatePPHFlat(const GameConfig &cfg) { 
    return cfg.getPphFlat(); 
}

int TaxManager::calculatePPHPct(Player &payer, const GameConfig &cfg) {
    return (payer.getNetWorth() * cfg.getPphPercentage()) / 100;
}

int TaxManager::getPBMFlat(const GameConfig &cfg) { 
    return cfg.getPbmFlat(); 
}