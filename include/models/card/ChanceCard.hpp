#ifndef CHANCECARD_HPP
#define CHANCECARD_HPP

#include "include/models/card/Card.hpp"

class StepbackCard : public ChanceCard {
    private:
        int boardSize;
    public:
        StepbackCard(int boardSize);
        void action(Player& player) override;
        std::string describe() const override;
};

class NearestStreetCard : public ChanceCard {
    private:
        int boardSize;
        std::vector<int> stationPositions;
    public:
        NearestStreetCard(int boardSize, std::vector<int> stationPositions);
        void action(Player& player) override;
        std::string describe() const override;
};

class JailCard : public ChanceCard {
    private:
        int jailPosition;
    public:
        JailCard(int jailPosition);
        void action(Player& player) override;
        std::string describe() const override;
};

#endif