#include "include/models/card/CommunityChest.hpp"
#include "include/models/player/Player.hpp"

HappyBirthdayCard::HappyBirthdayCard() {} 

void HappyBirthdayCard::action(Player& player)
{
    // No-op: transfer dari setiap pemain ke receiver ditangani oleh
    // CardManager::applyHappyBirthday agar tidak ter-double-count.
    (void)player;
}

std::string HappyBirthdayCard::describe() const 
{ 
    return "Ini adalah hari ulang tahun Anda. Dapatkan M100 dari setiap pemain."; 
}

SickCard::SickCard() {}

void SickCard::action(Player& player) 
{ 
    player += -700; 
}

std::string SickCard::describe() const 
{ 
    return "Biaya dokter. Bayar M700."; 
}

LegislativeCard::LegislativeCard() {}

void LegislativeCard::action(Player& player)
{
    // No-op: pembayaran dari payer ke setiap pemain ditangani oleh
    // CardManager::applyLegislative agar tidak ter-double-count.
    (void)player;
}

std::string LegislativeCard::describe() const 
{ 
    return "Anda mau nyaleg. Bayar M200 kepada setiap pemain."; 
}