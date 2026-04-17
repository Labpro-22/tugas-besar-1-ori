#ifndef FILEMANAGER_HPP
#define FILEMANAGER_HPP

#include <string>

#include "include/core/GameConfig.hpp"
#include "include/utils/exceptions/SaveLoadException.hpp"

class FileManager{
public:
    static void saveConfig(
        const std::string &file_path,
        const GameConfig::SaveState &state,
        const GameConfig::SavePermission &permission = GameConfig::SavePermission());

    static GameConfig::SaveState loadConfig(
        const std::string &file_path,
        const GameConfig::LoadPermission &permission = GameConfig::LoadPermission());
};

#endif
