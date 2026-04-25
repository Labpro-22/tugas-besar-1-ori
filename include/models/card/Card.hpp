#ifndef CARD_HPP
#define CARD_HPP

#include <string>
#include <vector>

class Player;

class Card {
    public:
        Card();
        virtual ~Card() = default;
        virtual void action(Player& player) = 0;
        virtual std::string describe() const { return ""; }
        virtual std::string getCardType() const = 0;
};

class ChanceCard : public Card {
    public:
        ChanceCard();
        std::string getCardType() const override { return "CHANCE"; }
};

class CommunityChestCard : public Card {
    public:
        CommunityChestCard();
        std::string getCardType() const override { return "COMMUNITY"; }
};

class SpecialPowerCard : public Card {
    public:
        SpecialPowerCard();
        std::string getCardType() const override { return "SKILL"; }
};
 
#endif