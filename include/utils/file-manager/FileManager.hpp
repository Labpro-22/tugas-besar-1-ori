#ifndef FILEMANAGER_HPP
#define FILEMANAGER_HPP

#include <string>
#include <vector>

#include "include/core/GameConfig.hpp"
#include "include/core/GameStates.hpp"
#include "include/utils/exceptions/SaveLoadException.hpp"

class Tile;

class FileManager {
public:
    static void saveConfig(
        const std::string &file_path,
        const GameStates::SaveState &state,
        const GameStates::SavePermission &permission = GameStates::SavePermission());

    static GameStates::SaveState loadConfig(
        const std::string &file_path,
        const GameStates::LoadPermission &permission = GameStates::LoadPermission());

    static std::vector<Tile*> loadBoard(const std::string &configDir);
    static void loadMiscConfig(const std::string &configDir, int &outMaxTurn, int &outInitBalance);
    static GameConfig loadGameConfig(const std::string &configDir);
};

#endif
