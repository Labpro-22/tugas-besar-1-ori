#include "include/models/dice/Dice.hpp"

void Dice::throwDice(){
    die1 = dist(engine);
    die2 = dist(engine);
    if(isDouble()) double_count++;
    // TODO: doubles exception
}

void Dice::setDice(int x, int y){
    die1 = x;
    die2 = y;
    if(isDouble()) double_count++;
    // TODO: doubles exception
}

bool Dice::isDouble(){
    return die1 == die2;
}

int Dice::getTotal(){
    return die1 + die2;
}

Dice::Dice() : engine(std::chrono::high_resolution_clock::now().time_since_epoch().count()), dist(1, 6){}