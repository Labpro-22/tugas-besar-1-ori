#ifndef BUTTON_HPP
#define BUTTON_HPP

#include <string>
#include "raylib.h"

class Button {
private:
    Texture2D texture;
    Texture2D hoverTexture;
    float x, y;
    float scale;
    float width, height;
    bool loaded;
    bool useHover;
    bool hasHitRect = false;
    float hitX = 0, hitY = 0, hitW = 0, hitH = 0;

public:
    Button();
    ~Button();

    void load(const std::string& path, float xPos, float yPos, float scl);
    void loadWithHover(const std::string& path, const std::string& hoverPath, float xPos, float yPos, float scl);
    void unload();

    void setPosition(float xPos, float yPos);
    void setScale(float scl);

    bool isHovered() const;
    bool isClicked() const;
    void draw() const;

    // Override clickable area (use when image has large transparent padding)
    void setHitRect(float hx, float hy, float hw, float hh);

    float getWidth() const { return width; }
    float getHeight() const { return height; }
    float getX() const { return x; }
    float getY() const { return y; }
};

#endif
