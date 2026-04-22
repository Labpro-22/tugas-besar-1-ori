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

namespace
{
    std::string trim(const std::string &text)
    {
        const std::string whitespace = " \t\r\n";
        const std::size_t start = text.find_first_not_of(whitespace);
        if (start == std::string::npos) return "";
        const std::size_t end = text.find_last_not_of(whitespace);
        return text.substr(start, end - start + 1);
    }

    int parseInt(const std::string &token, const std::string &field)
    {
        try
        {
            std::size_t processed = 0;
            const int value = std::stoi(token, &processed);
            if (processed != token.size())
                throw SaveLoadException("Nilai field " + field + " bukan bilangan bulat valid: " + token);
            return value;
        }
        catch (const std::invalid_argument &)
        {
            throw SaveLoadException("Nilai field " + field + " bukan bilangan bulat: " + token);
        }
        catch (const std::out_of_range &)
        {
            throw SaveLoadException("Nilai field " + field + " di luar batas integer: " + token);
        }
    }

    std::map<int, int> parseIntMap(const std::string &value, const std::string &field)
    {
        std::map<int, int> result;
        std::istringstream iss(value);
        std::string pair;
        while (std::getline(iss, pair, ','))
        {
            const std::string trimmed = trim(pair);
            if (trimmed.empty()) continue;
            const std::size_t colon = trimmed.find(':');
            if (colon == std::string::npos)
                throw SaveLoadException("Format field " + field + " harus 'key:value' dipisah koma: " + pair);
            const int key = parseInt(trim(trimmed.substr(0, colon)), field + ".key");
            const int val = parseInt(trim(trimmed.substr(colon + 1)), field + ".value");
            result[key] = val;
        }
        return result;
    }
}

GameConfig GameConfig::loadFromFile(const std::string &file_path)
{
    std::ifstream input(file_path);
    if (!input.is_open())
        throw SaveLoadException("Gagal membuka file konfigurasi: " + file_path);

    GameConfig config;
    std::string line;
    int line_number = 0;

    while (std::getline(input, line))
    {
        ++line_number;
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        const std::size_t equals = trimmed.find('=');
        if (equals == std::string::npos)
            throw SaveLoadException("Format konfigurasi invalid di baris " + std::to_string(line_number) + ": expected 'KEY=VALUE'.");

        const std::string key = trim(trimmed.substr(0, equals));
        const std::string value = trim(trimmed.substr(equals + 1));

        if (key == "GO_SALARY") config.go_salary = parseInt(value, key);
        else if (key == "JAIL_FINE") config.jail_fine = parseInt(value, key);
        else if (key == "PPH_FLAT") config.pph_flat = parseInt(value, key);
        else if (key == "PPH_PERCENTAGE") config.pph_percentage = parseInt(value, key);
        else if (key == "PBM_FLAT") config.pbm_flat = parseInt(value, key);
        else if (key == "MAX_TURN") config.max_turn = parseInt(value, key);
        else if (key == "INITIAL_BALANCE") config.initial_balance = parseInt(value, key);
        else if (key == "RAILROAD_RENT") config.railroad_rent = parseIntMap(value, key);
        else if (key == "UTILITY_MULTIPLIER") config.utility_multiplier = parseIntMap(value, key);
        else
            throw SaveLoadException("Key konfigurasi tidak dikenal di baris " + std::to_string(line_number) + ": " + key);
    }

    return config;
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
    const auto it = railroad_rent.find(owned_count);
    if (it == railroad_rent.end()) return 0;
    return it->second;
}

int GameConfig::getUtilityMultiplierForCount(int owned_count) const
{
    const auto it = utility_multiplier.find(owned_count);
    if (it == utility_multiplier.end()) return 0;
    return it->second;
}
