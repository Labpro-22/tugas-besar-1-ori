#include "include/core/PropertyManager.hpp"
#include "include/utils/exceptions/UnablePayBuildException.hpp"
#include "include/utils/exceptions/UnablePawnException.hpp"

#include <algorithm>
#include <iostream>
#include <set>

bool PropertyManager::mortgage(Player &owner, PropertyTile &tile) {
    if (tile.isMortgage()) {
        return false;
    }
    
    if (tile.getTileOwner() != &owner) {
        return false;
    }

    owner += tile.getMortgageValue();
    tile.setMortgageStatus(true);
    
    return true;
}

bool PropertyManager::build(Player &owner, PropertyTile &tile) {
    if (tile.getTileOwner() != &owner) {
        return false;
    }
    
    if (tile.getTileType() != "STREET") {
        return false;
    }
    
    if (tile.getLevel() >= 5) {
        return false;
    }

    int cost = (tile.getLevel() >= 4) ? tile.getHotelCost() : tile.getHouseCost();
    
    if (owner.getBalance() < cost) {
        throw UnablePayBuildException("Saldo tidak cukup untuk membangun.");
    }

    owner += -cost;
    tile.setLevel(tile.getLevel() + 1);
    
    return true;
}

bool PropertyManager::compensate(Player &player, Tile &tile) { 
    return false; 
}

bool PropertyManager::liquidate(Player &player, Tile &tile) { 
    return false; 
}

int PropertyManager::calculateMaxLiquidation(Player &player) {
    int total = player.getBalance();

    for (auto *p : player.getOwnedProperties()) {
        if (!p) {
            continue;
        }

        if (p->isMortgage()) {
            total += p->getBuyPrice();
        } else {
            total += p->getMortgageValue();
            if (p->getTileType() == "STREET" && p->getLevel() > 0) {
                int bldCost = (p->getLevel() >= 5) ? p->getHotelCost() : p->getHouseCost() * p->getLevel();
                total += bldCost / 2;
            }
        }
    }
    
    return total;
}

std::map<std::string, std::vector<PropertyManager::BuildOption>> PropertyManager::getBuildableOptions(Player &owner, Board &board) {
    std::map<std::string, std::vector<BuildOption>> result;
    std::set<std::string> processedGroups;

    for (auto *p : owner.getOwnedProperties()) {
        if (!p || p->getTileType() != "STREET") {
            continue;
        }

        std::string cg = p->getColorGroup();
        if (processedGroups.count(cg)) {
            continue;
        }

        processedGroups.insert(cg);
        if (!hasMonopoly(owner, board, cg)) {
            continue;
        }

        auto group = getGroupProperties(board, cg);
        std::vector<BuildOption> opts;

        for (auto *gp : group) {
            if (gp->getLevel() >= 5) {
                continue;
            }
            if (!canBuildOn(gp, owner, board)) {
                continue;
            }

            BuildOption opt;
            opt.prop = gp;
            int lvl = gp->getLevel();

            if (lvl == 0) {
                opt.levelStr = "0 rumah";
            } else if (lvl < 4) {
                opt.levelStr = std::to_string(lvl) + " rumah";
            } else {
                opt.levelStr = "4 rumah (siap hotel)";
            }

            opt.cost = (lvl >= 4) ? gp->getHotelCost() : gp->getHouseCost();
            opts.push_back(opt);
        }

        if (!opts.empty()) {
            result[cg] = opts;
        }
    }
    
    return result;
}

bool PropertyManager::canBuildOn(PropertyTile *prop, Player &owner, Board &board) {
    if (!prop || prop->getTileOwner() != &owner) {
        return false;
    }
    
    if (prop->getTileType() != "STREET") {
        return false;
    }
    
    if (prop->getLevel() >= 5) {
        return false;
    }

    std::string cg = prop->getColorGroup();
    if (!hasMonopoly(owner, board, cg)) {
        return false;
    }

    int minLvl = getMinLevelInGroup(board, cg);
    if (prop->getLevel() > minLvl) {
        return false;
    }

    if (prop->getLevel() == 4 && !allReadyForHotel(board, cg)) {
        return false;
    }

    return true;
}

std::vector<PropertyTile*> PropertyManager::getGroupProperties(Board &board, const std::string &colorGroup) {
    return board.getPropertiesByColorGroup(colorGroup);
}

int PropertyManager::getMinLevelInGroup(Board &board, const std::string &colorGroup) {
    auto group = board.getPropertiesByColorGroup(colorGroup);
    
    if (group.empty()) {
        return 0;
    }

    int minLvl = group[0]->getLevel();
    for (auto *p : group) {
        if (p->getLevel() < minLvl) {
            minLvl = p->getLevel();
        }
    }
    
    return minLvl;
}

bool PropertyManager::allReadyForHotel(Board &board, const std::string &colorGroup) {
    auto group = board.getPropertiesByColorGroup(colorGroup);
    for (auto *p : group) {
        if (p->getLevel() < 4) {
            return false;
        }
    }
    return true;
}

bool PropertyManager::hasMonopoly(Player &owner, Board &board, const std::string &colorGroup) {
    auto group = board.getPropertiesByColorGroup(colorGroup);
    
    if (group.empty()) {
        return false;
    }

    for (auto *p : group) {
        if (p->getTileOwner() != &owner) {
            return false;
        }
    }
    
    return true;
}

void PropertyManager::sellAllBuildingsInGroup(
    Player &owner, 
    Board &board, 
    const std::string &colorGroup,
    std::vector<std::tuple<std::string, int, int>> *outDetails
) {
    auto group = board.getPropertiesByColorGroup(colorGroup);
    
    for (auto *p : group) {
        if (p->getLevel() > 0) {
            int refund = 0;
            if (p->getLevel() >= 5) {
                refund = p->getHotelCost() / 2;
            } else {
                refund = (p->getHouseCost() * p->getLevel()) / 2;
            }

            if (outDetails) {
                outDetails->push_back({p->getTileCode(), refund, p->getLevel()});
            }

            owner += refund;
            p->setLevel(0);
        }
    }
}

bool PropertyManager::autoLiquidate(Player &p, Board &board, int targetAmount) {
    int needed = targetAmount - p.getBalance();
    if (needed <= 0) return true;

    for (auto *prop : p.getOwnedProperties()) {
        if (!prop || prop->isMortgage()) continue;
        
        if (prop->getTileType() == "STREET" && prop->getLevel() > 0) {
            int refund = (prop->getLevel() >= 5) ? prop->getHotelCost() / 2 : (prop->getHouseCost() * prop->getLevel()) / 2;
            p += refund;
            prop->setLevel(0);
            
            needed = targetAmount - p.getBalance();
            if (needed <= 0) return true;
        }
    }

    for (auto *prop : p.getOwnedProperties()) {
        if (!prop || prop->isMortgage()) continue;
        
        p += prop->getMortgageValue();
        prop->setMortgageStatus(true);
        
        needed = targetAmount - p.getBalance();
        if (needed <= 0) return true;
    }

    return p.getBalance() >= targetAmount;
}