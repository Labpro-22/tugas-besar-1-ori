#ifndef LANDINGPROCESSOR_HPP
#define LANDINGPROCESSOR_HPP

#include <string>
#include <vector>
#include "include/core/GameContext.hpp"

class GameState;
class Player;
class PropertyTile;

class LandingProcessor : public GameContext {
private:
    GameState &state;

public:
    explicit LandingProcessor(GameState &s);

    bool rollAndMove(Player &p, bool manual, int d1 = 0, int d2 = 0);
    void applyLanding(Player &p);
    void applyGoSalary(Player &p, int oldTile);

    void sendToJail(Player &p) override;
    void drawAndResolveChance(Player &p) override;
    void drawAndResolveCommunityChest(Player &p) override;
    void handleTax(Player &p, const std::string &taxType) override;
    void handlePropertyLanding(Player &p, PropertyTile &prop) override;
    void handleFestivalLanding(Player &p) override;
    void displayMessage(const std::string &msg) override;
    void addLog(Player &p, const std::string &action, const std::string &detail) override;
    int getDiceTotal() const override;
    int computeRent(PropertyTile &prop, int diceTotal) const override;
    std::vector<Player*> getActivePlayers() const override;
};

#endif
