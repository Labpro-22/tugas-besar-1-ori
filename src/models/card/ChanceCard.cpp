#include "include/models/card/ChanceCard.hpp"
#include "include/models/player/Player.hpp"

StepbackCard::StepbackCard(int boardSize) : boardSize(boardSize) {}

void StepbackCard::action(Player& player) 
{
    int cur = player.getCurrTile();
    int newPos = (cur - 3 + boardSize) % boardSize;
    player.setCurrTile(newPos);
}

std::string StepbackCard::describe() const 
{ 
    return "Mundur 3 petak."; 
}

NearestStreetCard::NearestStreetCard(int boardSize, std::vector<int> stationPositions)
    : boardSize(boardSize), stationPositions(stationPositions) 
{
}

void NearestStreetCard::action(Player& player) 
{
    int cur = player.getCurrTile();
    int nearest = -1;
    int minDist = boardSize + 1;
    
    for (int pos : stationPositions) 
    {
        int dist = (pos - cur + boardSize) % boardSize;
        if (dist > 0 && dist < minDist) 
        { 
            minDist = dist; 
            nearest = pos; 
        }
    }
    
    if (nearest >= 0) 
    {
        player.setCurrTile(nearest);
    }
}

std::string NearestStreetCard::describe() const 
{ 
    return "Pergi ke stasiun terdekat."; 
}

JailCard::JailCard(int jailPosition) : jailPosition(jailPosition) 
{
}

void JailCard::action(Player& player) 
{
    player.setCurrTile(jailPosition);
    player.setStatus("JAIL");
}

std::string JailCard::describe() const 
{ 
    return "Masuk Penjara."; 
}