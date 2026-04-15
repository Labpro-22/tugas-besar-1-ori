#ifndef SPECIALPOWERCARD_HPP
#define SPECIALPOWERCARD_HPP

#include "include/models/card/Card.hpp"

class MoveCard : public SpecialPowerCard {
    private:
        int value;
    public:
        MoveCard(int value);
        int getMoveValue() const;
        // void action(Player& player) override;
};

class DiscountCard : public SpecialPowerCard {
    private:
        int value;
    public:
        DiscountCard(int value);
        int getDiscountValue() const;
        // void action(Player& player) override;
};

class ShieldCard : public SpecialPowerCard {
    public:
        ShieldCard();
        // void action(Player& player) override;
};

class TeleportCard : public SpecialPowerCard {
    public:
        TeleportCard();
        // void action(Player& player) override;
};

class LassoCard : public SpecialPowerCard {
    public:
        LassoCard();
        // void action(Player& player) override;
};

class DemolitionCard : public SpecialPowerCard {
    public:
        DemolitionCard();
        // void action(Player& player) override;
};

#endif