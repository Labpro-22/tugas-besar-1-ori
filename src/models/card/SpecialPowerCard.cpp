#include "include/models/card/SpecialPowerCard.hpp"
#include "include/models/player/Player.hpp"

MoveCard::MoveCard(int value, int boardSize) : value(value), boardSize(boardSize) {}

int MoveCard::getMoveValue() const 
{ 
    return value; 
}

void MoveCard::action(Player& player) 
{
    int cur = player.getCurrTile();
    player.setCurrTile((cur + value) % boardSize);
}

std::string MoveCard::describe() const 
{ 
    return "MoveCard - Maju " + std::to_string(value) + " Petak"; 
}

DiscountCard::DiscountCard(int value) : value(value) {}

int DiscountCard::getDiscountValue() const 
{ 
    return value; 
}

void DiscountCard::action(Player& player) 
{ 
    player.setDiscountActive(static_cast<float>(value) / 100.0f); 
}

std::string DiscountCard::describe() const 
{ 
    return "DiscountCard - Diskon " + std::to_string(value) + "%"; 
}

ShieldCard::ShieldCard() {}

void ShieldCard::action(Player& player) 
{ 
    player.setShieldActive(true); 
}

std::string ShieldCard::describe() const 
{ 
    return "ShieldCard - Kebal tagihan/sanksi selama 1 turn"; 
}

TeleportCard::TeleportCard() {}

void TeleportCard::action(Player& player) {}

std::string TeleportCard::describe() const 
{ 
    return "TeleportCard - Pindah ke petak manapun"; 
}

LassoCard::LassoCard() {}

void LassoCard::action(Player& player) {}

std::string LassoCard::describe() const 
{ 
    return "LassoCard - Menarik lawan ke petak Anda"; 
}

DemolitionCard::DemolitionCard() {}

void DemolitionCard::action(Player& player) {}

std::string DemolitionCard::describe() const 
{ 
    return "DemolitionCard - Menghancurkan bangunan lawan"; 
}