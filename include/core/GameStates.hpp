#ifndef GAMECONFIG_HPP
#define GAMECONFIG_HPP

#include <string>
#include <vector>

class GameConfig
{
public:
    class CardState
    {
    public:
        std::string type;
        std::string value;
        std::string remaining_duration;
    };

    class PlayerState
    {
    public:
        std::string username;
        int money = 0;
        std::string tile_code;
        std::string status;
        std::vector<CardState> hand_cards;
    };

    class PropertyState
    {
    public:
        std::string tile_code;
        std::string type;
        std::string owner_username;
        std::string status;
        int festival_multiplier = 1;
        int festival_duration = 0;
        std::string building_count = "0";
    };

    class DeckState
    {
    public:
        std::vector<std::string> ability_deck;
    };

    class LogState
    {
    public:
        int turn = 0;
        std::string username;
        std::string action_type;
        std::string detail;
    };

    class SaveState
    {
    public:
        int current_turn = 0;
        int max_turn = 0;
        std::vector<PlayerState> players;
        std::vector<std::string> turn_order;
        std::string active_turn_player;
        std::vector<PropertyState> properties;
        DeckState deck;
        std::vector<LogState> logs;
    };

    class SavePermission
    {
    public:
        bool is_at_start_of_turn = true;
        bool has_rolled_dice = false;
        bool has_executed_action = false;
    };

    class LoadPermission
    {
    public:
        bool is_before_game_start = true;
    };
};

#endif
