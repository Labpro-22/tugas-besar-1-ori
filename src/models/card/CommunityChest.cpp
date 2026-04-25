#include "include/models/card/CommunityChest.hpp"
#include "include/models/player/Player.hpp"

HappyBirthdayCard::HappyBirthdayCard() {} 

void HappyBirthdayCard::action(Player& player) 
{ 
    player += 100; 
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
    player += -200; 
}

std::string LegislativeCard::describe() const 
{ 
    return "Anda mau nyaleg. Bayar M200 kepada setiap pemain."; 
}