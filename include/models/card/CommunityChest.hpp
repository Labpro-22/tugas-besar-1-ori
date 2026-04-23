#ifndef COMMUNITYCHEST_HPP
#define COMMUNITYCHEST_HPP

#include "include/models/card/Card.hpp"

class HappyBirthdayCard : public CommunityChestCard {
    public:
        HappyBirthdayCard();
        void action(Player& player) override;
        std::string describe() const override;
};

class SickCard : public CommunityChestCard {
    public:
        SickCard();
        void action(Player& player) override;
        std::string describe() const override;
};

class LegislativeCard : public CommunityChestCard {
    public:
        LegislativeCard();
        void action(Player& player) override;
        std::string describe() const override;
};

#endif