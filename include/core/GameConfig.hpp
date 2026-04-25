#ifndef GAMECONFIG_HPP
#define GAMECONFIG_HPP

#include <map>
#include <string>
#include <cstdio>

class GameConfig
{
public:
    static constexpr int MAX_PLAYERS  = 4;
    static constexpr int MAX_NAME_LEN = 16;

    int  playerCount = 2;
    char playerNames[MAX_PLAYERS][MAX_NAME_LEN + 1]{};

    GameConfig() {
        for (int i = 0; i < MAX_PLAYERS; i++)
            std::snprintf(playerNames[i], MAX_NAME_LEN + 1, "Player %d", i + 1);
    }

    GameConfig(int go_salary,
               int jail_fine,
               int pph_flat,
               int pph_percentage,
               int pbm_flat,
               int max_turn,
               int initial_balance,
               const std::map<int, int> &railroad_rent,
               const std::map<int, int> &utility_multiplier);

    static GameConfig loadFromFile(const std::string &file_path);

    int getGoSalary() const;
    int getJailFine() const;
    int getPphFlat() const;
    int getPphPercentage() const;
    int getPbmFlat() const;
    int getMaxTurn() const;
    int getInitialBalance() const;
    const std::map<int, int> &getRailroadRent() const;
    const std::map<int, int> &getUtilityMultiplier() const;

    int getRailroadRentForCount(int owned_count) const;
    int getUtilityMultiplierForCount(int owned_count) const;

private:
    int go_salary = 0;
    int jail_fine = 0;
    int pph_flat = 0;
    int pph_percentage = 0;
    int pbm_flat = 0;
    int max_turn = 0;
    int initial_balance = 0;
    std::map<int, int> railroad_rent;
    std::map<int, int> utility_multiplier;
};

#endif
