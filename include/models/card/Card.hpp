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
};

class ChanceCard : public Card {
    public:
        ChanceCard();
};

class CommunityChestCard : public Card {
    public:     
        CommunityChestCard();
};

class SpecialPowerCard : public Card {
    public:
        SpecialPowerCard();
};
 
#endif