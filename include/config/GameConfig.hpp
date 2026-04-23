#ifndef GAMECONFIG_HPP
#define GAMECONFIG_HPP

struct GameConfig {
    static constexpr int MAX_PLAYERS = 6;
    static constexpr int MAX_NAME_LEN = 16;

    int playerCount = 2;
    char playerNames[MAX_PLAYERS][MAX_NAME_LEN + 1]{};

    GameConfig() {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            snprintf(playerNames[i], MAX_NAME_LEN + 1, "Player %d", i + 1);
        }
    }
};

#endif