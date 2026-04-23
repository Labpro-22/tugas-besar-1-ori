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

    bool isTextBtn;
    std::string textLabel;
    Color bgColor;
    Color hoverColor;
    Color textColor;
    bool active;

public:
    Button();
    ~Button();

    void load(const std::string& path, float xPos, float yPos, float scl);
    void loadWithHover(const std::string& path, const std::string& hoverPath, float xPos, float yPos, float scl);
    void loadAsText(const std::string& label, float xPos, float yPos, float w, float h,
                    Color bg = {148, 73, 68, 255}, Color hover = {170, 93, 88, 255}, Color fg = WHITE);
    void unload();

    void setPosition(float xPos, float yPos);
    void setScale(float scl);
    void setActive(bool a);
    bool isActive() const;

    bool isHovered() const;
    bool isClicked() const;
    void draw() const;

    void setHitRect(float hx, float hy, float hw, float hh);

    float getWidth() const { return width; }
    float getHeight() const { return height; }
    float getX() const { return x; }
    float getY() const { return y; }
};

#endif