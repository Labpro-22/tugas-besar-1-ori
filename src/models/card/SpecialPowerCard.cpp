#include "include/models/card/SpecialPowerCard.hpp"

MoveCard::MoveCard(int value) : SpecialPowerCard(), value(value) {}

int MoveCard::getMoveValue() const {
    return value;
}

// void MoveCard::action(Player& player) override;


DiscountCard::DiscountCard(int value) : SpecialPowerCard(), value(value) {}

int DiscountCard::getDiscountValue() const {
    return value;
}

// void DiscountCard::action(Player& player) override;


ShieldCard::ShieldCard() : SpecialPowerCard() {}

// void ShieldCard::action(Player& player) override;


TeleportCard::TeleportCard() : SpecialPowerCard() {}

// void TeleportCard::action(Player& player) override;


LassoCard::LassoCard() : SpecialPowerCard() {}

// void LassoCard::action(Player& player) override;


DemolitionCard::DemolitionCard() : SpecialPowerCard() {}

// void DemolitionCard::action(Player& player) override;