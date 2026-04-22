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
        const GameConfig::SaveState &state,
        const GameConfig::SavePermission &permission = GameConfig::SavePermission());

    static GameConfig::SaveState loadConfig(
        const std::string &file_path,
        const GameConfig::LoadPermission &permission = GameConfig::LoadPermission());

    // ── board construction from config dir ─────────────────────────────────
    // Reads board.txt (tile layout) + property.txt (property data).
    // Caller owns the returned Tile* pointers.
    static std::vector<Tile*> loadBoard(const std::string &configDir);

    // ── misc game settings (misc.txt) ──────────────────────────────────────
    static void loadMiscConfig(const std::string &configDir,
                               int &outMaxTurn, int &outInitBalance);
};

#endif
