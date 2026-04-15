#ifndef CARDDECK_HPP
#define CARDDECK_HPP

#include <vector>

template <typename T>
class CardDeck {
    private:
        std::vector<T*> draw_pile;
        std::vector<T*> discard_pile;
    public:
        T* draw();
        T* discard();
        void shuffle();
        void refillFromDiscard();
};

#endif