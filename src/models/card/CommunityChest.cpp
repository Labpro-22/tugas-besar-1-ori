#include "include/models/card/CommunityChest.hpp"
#include "include/models/player/Player.hpp"
#include <algorithm>
#include <iostream>

HappyBirthdayCard::HappyBirthdayCard() : CommunityChestCard() {}
std::string HappyBirthdayCard::describe() const { return "Ulang tahun! Terima M100 dari setiap pemain lain"; }
std::string SickCard::describe() const { return "Sakit! Bayar M700 biaya rumah sakit"; }
std::string LegislativeCard::describe() const { return "Biaya legislatif! Bayar M200 ke setiap pemain lain"; }

void HappyBirthdayCard::action(Player &player)
{
    // Multi-player transfer ditangani oleh CardManager.
    (void)player;
}

SickCard::SickCard() : CommunityChestCard() {}

void SickCard::action(Player &player)
{
    int payment = 700;
    if (player.getBalance() >= payment) {
        player += -payment;
        std::cout << player.getUsername() << " membayar M" << payment << " ke Bank. Sisa Uang = M" << player.getBalance() << ".\n";
    } else {
        // Saldo tidak cukup
        std::cout << player.getUsername() << " tidak mampu membayar biaya dokter! (M" << payment << ")\n";
        std::cout << "Uang saat ini: M" << player.getBalance() << "\n";
        // Bayar sebisa mungkin, sisanya ditangani di applyLanding
        int actual = player.getBalance();
        if (actual > 0) player += -actual;
    }
}

LegislativeCard::LegislativeCard() : CommunityChestCard() {}

void LegislativeCard::action(Player &player)
{
    // Multi-player transfer ditangani oleh CardManager.
    (void)player;
}