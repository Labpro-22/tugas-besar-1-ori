#include "include/models/card/CommunityChest.hpp"
#include "include/models/player/Player.hpp"

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
    player.addBalance(-700);
}

LegislativeCard::LegislativeCard() : CommunityChestCard() {}

void LegislativeCard::action(Player &player)
{
    // Multi-player transfer ditangani oleh CardManager.
    (void)player;
}