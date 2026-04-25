#ifndef GAMECONTEXT_HPP
#define GAMECONTEXT_HPP
#include <string>
#include <vector>

class Player;
class PropertyTile;

class GameContext {
public:
    virtual ~GameContext() = default;
    virtual void sendToJail(Player &p) = 0;
    virtual void drawAndResolveChance(Player &p) = 0;
    virtual void drawAndResolveCommunityChest(Player &p) = 0;
    virtual void handleTax(Player &p, const std::string &taxType) = 0;
    virtual void handlePropertyLanding(Player &p, PropertyTile &prop) = 0;
    virtual void handleFestivalLanding(Player &p) = 0;
    virtual void displayMessage(const std::string &msg) = 0;
    virtual void addLog(Player &p, const std::string &action, const std::string &detail) = 0;
    virtual int getDiceTotal() const = 0;
    virtual int computeRent(PropertyTile &prop, int diceTotal) const = 0;
    virtual std::vector<Player*> getActivePlayers() const = 0;
};
#endif