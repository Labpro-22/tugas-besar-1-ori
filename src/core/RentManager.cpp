#include "include/core/RentManager.hpp"

bool RentManager::payRent(Player &buyer, Player &owner, Tile &tile)
{
    auto *prop = dynamic_cast<PropertyTile*>(&tile);
    if (!prop) {
        return false;
    }

    int rent = prop->calculateRent();
    if (buyer.getBalance() < rent) {
        return false;
    }

    buyer += -rent;
    owner += rent;
    return true;
}

int RentManager::computeRent(
    PropertyTile &prop, 
    int diceTotal, 
    const GameConfig &cfg, 
    const std::vector<Tile*> &allTiles
) {
    std::string type = prop.getTileType();
    Player *owner = prop.getTileOwner();
    
    if (!owner || prop.isMortgage()) {
        return 0;
    }

    int baseRent = 0;

    if (type == "STREET") {
        int lvl = prop.getLevel();
        auto rents = prop.getRentPrice();
        
        if (lvl >= 0 && lvl < static_cast<int>(rents.size())) {
            baseRent = rents[lvl];
        }

        if (prop.isMonopolized() && lvl == 0) {
            baseRent *= 2;
        }
    } else if (type == "RAILROAD") {
        int cnt = countRailroadsOwnedBy(owner, allTiles);
        baseRent = cfg.getRailroadRentForCount(cnt);
    } else if (type == "UTILITY") {
        int cnt = countUtilitiesOwnedBy(owner, allTiles);
        int mult = cfg.getUtilityMultiplierForCount(cnt);
        baseRent = diceTotal * mult;
    }

    if (prop.getFestivalDuration() > 0) {
        baseRent *= prop.getFestivalMultiplier();
    }

    return baseRent;
}

int RentManager::countRailroadsOwnedBy(Player *owner, const std::vector<Tile*> &allTiles) {
    int cnt = 0;
    
    for (auto *t : allTiles) {
        auto *p = dynamic_cast<PropertyTile*>(t);
        if (p && p->getTileType() == "RAILROAD" && p->getTileOwner() == owner) {
            cnt++;
        }
    }
    
    return cnt;
}

int RentManager::countUtilitiesOwnedBy(Player *owner, const std::vector<Tile*> &allTiles) {
    int cnt = 0;
    
    for (auto *t : allTiles) {
        auto *p = dynamic_cast<PropertyTile*>(t);
        if (p && p->getTileType() == "UTILITY" && p->getTileOwner() == owner) {
            cnt++;
        }
    }
    
    return cnt;
}