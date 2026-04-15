#ifndef CHANCECARD_HPP
#define CHANCECARD_HPP

#include "include/models/card/Card.hpp"

class StepbackCard : public ChanceCard {
    public:
        StepbackCard();
        // void action(Player& player) override;
};

class NearestStreetCard : public ChanceCard {
    public:
        NearestStreetCard();
        // void action(Player& player) override;
};

class JailCard : public ChanceCard {
    public:
        JailCard();
        // void action(Player& player) override;
};

#endif