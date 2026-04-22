#ifndef FILEMANAGER_HPP
#define FILEMANAGER_HPP

#include <string>
#include <vector>

#include "include/core/GameStates.hpp"
#include "include/utils/exceptions/SaveLoadException.hpp"

class Tile;

class FileManager {
public:
    // ── game state save / load ──────────────────────────────────────────────
    static void saveConfig(
        const std::string &file_path,
        const GameStates::SaveState &state,
        const GameStates::SavePermission &permission = GameStates::SavePermission());

    static GameStates::SaveState loadConfig(
        const std::string &file_path,
        const GameStates::LoadPermission &permission = GameStates::LoadPermission());
};

#endif
