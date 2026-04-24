#include "include/models/player/Bot.hpp"

Bot::Bot(std::string username) : Player(username) {}
void Bot::move() {}

PropertyTile* Bot::chooseFestivalProperty(const std::vector<PropertyTile*> &props) {
    PropertyTile *best = nullptr;
    int maxRent = 0;
    for (auto *p : props) {
        if (p && !p->isMortgage()) {
            int r = p->calculateRent();
            if (r > maxRent) { maxRent = r; best = p; }
        }
    }
    return best;
}

bool Bot::shouldBuyStreet(int balance, int price) {
    return balance >= price * 2;
}
