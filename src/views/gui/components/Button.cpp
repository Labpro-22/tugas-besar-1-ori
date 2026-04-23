#include "Button.hpp"

Button::Button() : texture{}, hoverTexture{}, x(0), y(0), scale(1.0f),
    width(0), height(0), loaded(false), useHover(false) {}

Button::~Button() {
    unload();
}

void Button::load(const std::string& path, float xPos, float yPos, float scl) {
    unload();
    texture = LoadTexture(path.c_str());
    x = xPos;
    y = yPos;
    scale = scl;
    width = texture.width * scale;
    height = texture.height * scale;
    loaded = true;
    useHover = false;
}

void Button::loadWithHover(const std::string& path, const std::string& hoverPath,
                           float xPos, float yPos, float scl) {
    unload();
    texture = LoadTexture(path.c_str());
    if (texture.id == 0) {
        loaded = false;
        useHover = false;
        return;
    }
    hoverTexture = LoadTexture(hoverPath.c_str());
    x = xPos;
    y = yPos;
    scale = scl;
    width = texture.width * scale;
    height = texture.height * scale;
    loaded = true;
    useHover = (hoverTexture.id > 0);
}

void Button::unload() {
    if (loaded) {
        UnloadTexture(texture);
        if (useHover) UnloadTexture(hoverTexture);
        loaded = false;
        useHover = false;
    }
}

void Button::setPosition(float xPos, float yPos) {
    x = xPos;
    y = yPos;
}

void Button::setScale(float scl) {
    scale = scl;
    if (loaded) {
        width = texture.width * scale;
        height = texture.height * scale;
    }
}

void Button::setHitRect(float hx, float hy, float hw, float hh) {
    hasHitRect = true; hitX = hx; hitY = hy; hitW = hw; hitH = hh;
}

bool Button::isHovered() const {
    if (!loaded) return false;
    Vector2 mouse = GetMousePosition();
    float cx = hasHitRect ? hitX : x;
    float cy = hasHitRect ? hitY : y;
    float cw = hasHitRect ? hitW : width;
    float ch = hasHitRect ? hitH : height;
    return (mouse.x >= cx && mouse.x <= cx + cw &&
            mouse.y >= cy && mouse.y <= cy + ch);
}

bool Button::isClicked() const {
    return isHovered() && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

void Button::draw() const {
    if (!loaded) return;
    const Texture2D& tex = (useHover && isHovered()) ? hoverTexture : texture;
    DrawTextureEx(tex, {x, y}, 0.0f, scale, WHITE);
}
