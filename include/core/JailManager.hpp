#ifndef JAILMANAGER_HPP
#define JAILMANAGER_HPP

#include "include/models/player/Player.hpp"

class JailManager
{
public:
    static bool goToJail(Player &player, int jail_position);
};

#endif