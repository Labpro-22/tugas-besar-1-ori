#include "views/gui/components/Button.hpp"

Button::Button() : texture{}, hoverTexture{}, x(0), y(0), scale(1.0f),
    width(0), height(0), loaded(false), useHover(false),
    isTextBtn(false), bgColor({148,73,68,255}), hoverColor({170,93,88,255}), textColor(WHITE), active(false) {}

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
    isTextBtn = false;
    disabled = false;
}

void Button::loadWithHover(const std::string& path, const std::string& hoverPath,
                           float xPos, float yPos, float scl) {
    unload();
    texture = LoadTexture(path.c_str());
    if (texture.id == 0) {
        loaded = false;
        useHover = false;
        disabled = false;
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
    isTextBtn = false;
    disabled = false;
}

void Button::loadAsText(const std::string& label, float xPos, float yPos, float w, float h,
                        Color bg, Color hover, Color fg) {
    textLabel = label;
    x = xPos;
    y = yPos;
    width = w;
    height = h;
    bgColor = bg;
    hoverColor = hover;
    textColor = fg;
    loaded = true;
    isTextBtn = true;
    useHover = false;
    disabled = false;
}

void Button::unload() {
    if (loaded && !isTextBtn) {
        UnloadTexture(texture);
        if (useHover) UnloadTexture(hoverTexture);
    }
    loaded = false;
    useHover = false;
    isTextBtn = false;
}

void Button::setPosition(float xPos, float yPos) {
    x = xPos;
    y = yPos;
}

void Button::setScale(float scl) {
    scale = scl;
    if (loaded && !isTextBtn) {
        width = texture.width * scale;
        height = texture.height * scale;
    }
}

void Button::setColors(Color bg, Color hover, Color fg) {
    bgColor = bg;
    hoverColor = hover;
    textColor = fg;
}

void Button::setActive(bool a)    { active = a; }
bool Button::isActive() const     { return active; }
void Button::setDisabled(bool d)  { disabled = d; }
bool Button::isDisabled() const   { return disabled; }

void Button::setHitRect(float hx, float hy, float hw, float hh) {
    hasHitRect = true;
    hitX = hx; hitY = hy; hitW = hw; hitH = hh;
}

bool Button::isHovered() const {
    if (!loaded || disabled) return false;
    Vector2 mouse = GetMousePosition();
    if (hasHitRect)
        return (mouse.x >= hitX && mouse.x <= hitX + hitW &&
                mouse.y >= hitY && mouse.y <= hitY + hitH);
    return (mouse.x >= x && mouse.x <= x + width &&
            mouse.y >= y && mouse.y <= y + height);
}

bool Button::isClicked() const {
    return !disabled && isHovered() && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

void Button::draw() const {
    if (!loaded) return;
    if (isTextBtn) {
        if (disabled) {
            // Disabled: dimmed background + gray text and outline
            DrawRectangleRec({x, y, width, height}, {200, 190, 180, 180});
            DrawRectangleLinesEx({x, y, width, height}, 1, {170, 170, 170, 180});
            int fontSize = static_cast<int>(height * 0.5f);
            int tw = MeasureText(textLabel.c_str(), fontSize);
            DrawText(textLabel.c_str(), static_cast<int>(x + (width - tw) / 2.0f),
                     static_cast<int>(y + (height - fontSize) / 2.0f), fontSize, {170, 170, 170, 180});
        } else if (active) {
            DrawRectangleRec({x, y, width, height}, {255, 235, 202, 255});
            DrawRectangleLinesEx({x, y, width, height}, 1, {120, 60, 55, 255});
            int fontSize = static_cast<int>(height * 0.5f);
            int tw = MeasureText(textLabel.c_str(), fontSize);
            DrawText(textLabel.c_str(), static_cast<int>(x + (width - tw) / 2.0f),
                     static_cast<int>(y + (height - fontSize) / 2.0f), fontSize, {80, 40, 35, 255});
        } else {
            Color bg = isHovered() ? hoverColor : bgColor;
            DrawRectangleRec({x, y, width, height}, bg);
            DrawRectangleLinesEx({x, y, width, height}, 1, Fade(GOLD, 0.6f));
            int fontSize = static_cast<int>(height * 0.5f);
            int tw = MeasureText(textLabel.c_str(), fontSize);
            DrawText(textLabel.c_str(), static_cast<int>(x + (width - tw) / 2.0f),
                     static_cast<int>(y + (height - fontSize) / 2.0f), fontSize, textColor);
        }
    } else {
        const Texture2D& tex = (useHover && isHovered() && !disabled) ? hoverTexture : texture;
        Color tint = disabled ? Fade(WHITE, 0.5f) : WHITE;
        DrawTextureEx(tex, {x, y}, 0.0f, scale, tint);
    }
}