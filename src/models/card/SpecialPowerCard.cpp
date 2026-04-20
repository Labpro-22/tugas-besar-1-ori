#include "include/models/card/SpecialPowerCard.hpp"
#include "include/models/player/Player.hpp"
#include <iostream>

MoveCard::MoveCard(int value, int boardSize) : SpecialPowerCard(), value(value), boardSize(boardSize) {}

int MoveCard::getMoveValue() const { return value; }

void MoveCard::action(Player& player) {
    int currentTile = player.getCurrTile();
    int newTile = (currentTile + value) % boardSize;
    player.setCurrTile(newTile);

    std::cout << player.getUsername() << " menggunakan MoveCard! "
              << "Bergerak maju " << value << " petak ke petak " << newTile << "." << std::endl;
}

DiscountCard::DiscountCard(int value) : SpecialPowerCard(), value(value) {}

int DiscountCard::getDiscountValue() const { return value; }

void DiscountCard::action(Player& player) {
    float discountFraction = static_cast<float>(value) / 100.0f;
    player.setDiscountActive(discountFraction);

    std::cout << player.getUsername() << " menggunakan DiscountCard! "
              << "Diskon " << value << "% aktif untuk giliran ini." << std::endl;
}

ShieldCard::ShieldCard() : SpecialPowerCard() {}

void ShieldCard::action(Player& player) {
    player.setShieldActive(true);

    std::cout << player.getUsername() << " menggunakan ShieldCard! "
              << "Terlindungi dari tagihan dan sanksi giliran ini." << std::endl;
}

TeleportCard::TeleportCard() : SpecialPowerCard() {}

void TeleportCard::action(Player& player) {
    std::cout << player.getUsername() << " menggunakan TeleportCard! "
              << "Berpindah ke petak yang dipilih." << std::endl;
}

LassoCard::LassoCard() : SpecialPowerCard() {}

void LassoCard::action(Player& player) {
    std::cout << player.getUsername() << " menggunakan LassoCard! "
              << "Menarik lawan ke petak " << player.getCurrTile() << "." << std::endl;
}

DemolitionCard::DemolitionCard() : SpecialPowerCard() {}

void DemolitionCard::action(Player& player) {
    std::cout << player.getUsername() << " menggunakan DemolitionCard! "
              << "Menghancurkan properti lawan." << std::endl;
}