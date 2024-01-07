#pragma once
#include <SFML/Graphics.hpp>
#include <cstdint>
#include <optional>
#include <vector>
#include <string>

#include "StandardMove.h"
#include "Game.h"

#define LIGHT_SQUARE_COLOR sf::Color(0xf0, 0xd9, 0xb5)
#define DARK_SQUARE_COLOR sf::Color(0xb5, 0x88, 0x63)

#define LIGHT_CURRENTLY_SELECTED sf::Color(0xdc, 0xc3, 0x4b)
#define DARK_CURRENTLY_SELECTED LIGHT_CURRENTLY_SELECTED

#define LIGHT_AVAILABLE_TARGET LIGHT_SQUARE_COLOR * sf::Color(210, 210, 200)
#define DARK_AVAILABLE_TARGET DARK_SQUARE_COLOR * sf::Color(200, 200, 200)

#define LIGHT_PREVIOUS_MOVE sf::Color(0xA0, 0xD0, 0xE0) * sf::Color(200, 200, 200)
#define DARK_PREVIOUS_MOVE LIGHT_PREVIOUS_MOVE

#define GENERATE_ONLY_QUEEN_PROMOTIONS true

class DrawableBoard : public Game, public sf::Drawable
{
public:
    DrawableBoard(sf::Vector2f position, bool whiteOnBottom);

    sf::Vector2f getPosition();

    void mouseDrag(sf::Vector2f position);

    void mouseDown(sf::Vector2f position);

    void mouseUp(sf::Vector2f position);

    bool bottomPlayerToMove() const;

    std::optional<StandardMove> getLastMovePlayed() const;

    void reset(bool whiteOnBottom);

private:
    sf::Vector2f boardPosition;

    sf::Vector2f squarePositions[64];

    sf::Texture peiceTextures[15];

    sf::Sprite hoveringPeice;

    int hiddenSquare;

    bool bottomIsWhite;

    std::optional<int> currentlySelected;

    void initGraphicalMembers();

    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
};

