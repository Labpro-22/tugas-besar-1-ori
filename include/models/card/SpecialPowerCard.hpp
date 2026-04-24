#ifndef SPECIALPOWERCARD_HPP
#define SPECIALPOWERCARD_HPP

#include "include/models/card/Card.hpp"

class MoveCard : public SpecialPowerCard {
    private:
        int value;
        int boardSize;
    public:
        MoveCard(int value, int boardSize);
        int getMoveValue() const;
        void action(Player& player) override;
        std::string describe() const override;
        std::string getCardType() const override { return "MOVE"; }
};

class DiscountCard : public SpecialPowerCard {
    private:
        int value;
    public:
        DiscountCard(int value);
        int getDiscountValue() const;
        void action(Player& player) override;
        std::string describe() const override;
        std::string getCardType() const override { return "DISCOUNT"; }
};

class ShieldCard : public SpecialPowerCard {
    public:
        ShieldCard();
        void action(Player& player) override;
        std::string describe() const override;
        std::string getCardType() const override { return "SHIELD"; }
};

class TeleportCard : public SpecialPowerCard {
    public:
        TeleportCard();
        void action(Player& player) override;
        std::string describe() const override;
        std::string getCardType() const override { return "TELEPORT"; }
};

class LassoCard : public SpecialPowerCard {
    public:
        LassoCard();
        void action(Player& player) override;
        std::string describe() const override;
        std::string getCardType() const override { return "LASSO"; }
};

class DemolitionCard : public SpecialPowerCard {
    public:
        DemolitionCard();
        void action(Player& player) override;
        std::string describe() const override;
        std::string getCardType() const override { return "DEMOLITION"; }
};

#endif