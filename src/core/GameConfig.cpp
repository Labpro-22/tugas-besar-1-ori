#include "include/core/GameConfig.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "include/utils/exceptions/SaveLoadException.hpp"

GameConfig::GameConfig(int go_salary,
                       int jail_fine,
                       int pph_flat,
                       int pph_percentage,
                       int pbm_flat,
                       int max_turn,
                       int initial_balance,
                       const std::map<int, int> &railroad_rent,
                       const std::map<int, int> &utility_multiplier)
    : go_salary(go_salary),
      jail_fine(jail_fine),
      pph_flat(pph_flat),
      pph_percentage(pph_percentage),
      pbm_flat(pbm_flat),
      max_turn(max_turn),
      initial_balance(initial_balance),
      railroad_rent(railroad_rent),
      utility_multiplier(utility_multiplier) {}

GameConfig GameConfig::loadFromFile(const std::string &file_path)
{
    std::map<int,int> rr, ut;
    return GameConfig(0,0,0,0,0,0,0,rr,ut);
}

int GameConfig::getGoSalary() const { return go_salary; }
int GameConfig::getJailFine() const { return jail_fine; }
int GameConfig::getPphFlat() const { return pph_flat; }
int GameConfig::getPphPercentage() const { return pph_percentage; }
int GameConfig::getPbmFlat() const { return pbm_flat; }
int GameConfig::getMaxTurn() const { return max_turn; }
int GameConfig::getInitialBalance() const { return initial_balance; }
const std::map<int, int> &GameConfig::getRailroadRent() const { return railroad_rent; }
const std::map<int, int> &GameConfig::getUtilityMultiplier() const { return utility_multiplier; }

int GameConfig::getRailroadRentForCount(int owned_count) const
{
    auto it = railroad_rent.find(owned_count);
    return (it != railroad_rent.end()) ? it->second : 0;
}

int GameConfig::getUtilityMultiplierForCount(int owned_count) const
{
    auto it = utility_multiplier.find(owned_count);
    return (it != utility_multiplier.end()) ? it->second : 0;
}
