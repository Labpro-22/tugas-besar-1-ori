#ifndef CARDDECK_HPP
#define CARDDECK_HPP

#include <vector>
#include <algorithm>
#include <random>
#include <chrono>

template <typename T>
class CardDeck {
    private:
        std::vector<T*> draw_pile;
        std::vector<T*> discard_pile;
        std::mt19937 rng;

    public:
        CardDeck() : rng(std::chrono::steady_clock::now().time_since_epoch().count()) {}

        void addCard(T* card) {
            draw_pile.push_back(card);
        }

        T* draw() {
            if (draw_pile.empty()) {
                refillFromDiscard();
            }
            if (draw_pile.empty()) {
                return nullptr;
            }
            T* card = draw_pile.back();
            draw_pile.pop_back();
            return card;
        }

        T* discard() {
            if (draw_pile.empty()) {
                return nullptr;
            }
            T* card = draw_pile.back();
            draw_pile.pop_back();
            discard_pile.push_back(card);
            return card;
        }

        void discardCard(T* card) {
            if (card != nullptr) {
                discard_pile.push_back(card);
            }
        }

        void shuffle() {
            std::shuffle(draw_pile.begin(), draw_pile.end(), rng);
        }

        void refillFromDiscard() {
            for (T* card : discard_pile) {
                draw_pile.push_back(card);
            }
            discard_pile.clear();
            shuffle();
        }

        void returnAndShuffle(T* card) {
            if (card != nullptr) {
                draw_pile.push_back(card);
                shuffle();
            }
        }

        bool isEmpty() const {
            return draw_pile.empty();
        }

        int drawPileSize() const {
            return static_cast<int>(draw_pile.size());
        }

        int discardPileSize() const {
            return static_cast<int>(discard_pile.size());
        }
};

#endif