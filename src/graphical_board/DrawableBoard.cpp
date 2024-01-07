#include "DrawableBoard.h"
#include <SFML/Graphics.hpp>
#include <cstdint>
#include <optional>
#include <vector>
#include <stack>
#include <forward_list>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <sstream>
#include <cctype>

#include "precomputed_chess_data.h"
#include "StandardMove.h"
#include "chesshelpers.h"

#define IS_LIGHT_SQUARE(x) ((x + (x / 8) % 2) % 2 == 1)

typedef std::uint_fast64_t uint64;

// PUBLIC METHODS
DrawableBoard::DrawableBoard(sf::Vector2f position, bool whiteOnBottom)
{
    boardPosition = position;
    bottomIsWhite = whiteOnBottom;

    // Load peice textures
    sf::Image empty;
    empty.create(120, 120, sf::Color(0, 0, 0, 0));
    peiceTextures[0].loadFromImage(empty);
    peiceTextures[1].loadFromFile("assets/120px/white_pawn.png");
    peiceTextures[2].loadFromFile("assets/120px/white_knight.png");
    peiceTextures[3].loadFromFile("assets/120px/white_bishop.png");
    peiceTextures[4].loadFromFile("assets/120px/white_rook.png");
    peiceTextures[5].loadFromFile("assets/120px/white_queen.png");
    peiceTextures[6].loadFromFile("assets/120px/white_king.png");
    peiceTextures[7].loadFromImage(empty);
    peiceTextures[8].loadFromImage(empty);
    peiceTextures[9].loadFromFile("assets/120px/black_pawn.png");
    peiceTextures[10].loadFromFile("assets/120px/black_knight.png");
    peiceTextures[11].loadFromFile("assets/120px/black_bishop.png");
    peiceTextures[12].loadFromFile("assets/120px/black_rook.png");
    peiceTextures[13].loadFromFile("assets/120px/black_queen.png");
    peiceTextures[14].loadFromFile("assets/120px/black_king.png");

    hoveringPeice.setTexture(peiceTextures[0]);
    hoveringPeice.setOrigin(60, 60);

    loadStartingPosition();
    initGraphicalMembers();
}

sf::Vector2f DrawableBoard::getPosition()
{
    return boardPosition;
}

void DrawableBoard::mouseDrag(sf::Vector2f position)
{
    hoveringPeice.setPosition(position);
}

void DrawableBoard::mouseDown(sf::Vector2f position)
{
    sf::Vector2f relativePosition = position - boardPosition;
    int x = static_cast<int>(relativePosition.x) / 120;
    int y = static_cast<int>(relativePosition.y) / 120;
    if (x < 0 || x > 7 || y < 0 || y > 7) {
        currentlySelected.reset();
        return;
    }
    int index = bottomIsWhite ? (7 - y) * 8 + x : y * 8 + (7 - x);


    if (peices[index] && peices[index] >> 3 == totalHalfmoves % 2) {
        // Selected a new peice
        currentlySelected = index;
        hoveringPeice.setTexture(peiceTextures[peices[index]]);
        hiddenSquare = index;
        hoveringPeice.setPosition(position);
        return;

    }

    if (currentlySelected.has_value()) {
        // Selecting a target for the peice
        int s = currentlySelected.value();

        try {
            int promotion = 0;
            if ((peices[s] & 0b111) == PAWN && ((index >> 3) == 0 || (index >> 3) == 7)) {
                promotion = QUEEN;
            }
            inputMove(StandardMove(s, index, promotion));
            hoveringPeice.setTexture(peiceTextures[0]);
            currentlySelected.reset();
        }
        catch (const std::runtime_error& error) {}

    }
}

void DrawableBoard::mouseUp(sf::Vector2f position)
{
    if (!currentlySelected.has_value()) {
        // No peice to move
        return;
    }

    int s = currentlySelected.value();

    // Clear hovering peice
    hiddenSquare = -1;
    hoveringPeice.setTexture(peiceTextures[0]);

    sf::Vector2f relativePosition = position - boardPosition;
    int x = static_cast<int>(relativePosition.x) / 120;
    int y = static_cast<int>(relativePosition.y) / 120;
    if (x < 0 || x > 7 || y < 0 || y > 7) {
        currentlySelected.reset();
        return;
    }
    int index = bottomIsWhite ? (7 - y) * 8 + x : y * 8 + (7 - x);

    try {
        // Selecting a target for the peice
        int promotion = 0;
        if ((peices[s] & 0b111) == PAWN && ((index >> 3) == 0 || (index >> 3) == 7)) {
            promotion = QUEEN;
        }
        inputMove(StandardMove(s, index, promotion));
        hoveringPeice.setTexture(peiceTextures[0]);
        currentlySelected.reset();
    }
    catch (const std::runtime_error& error) {}
}

bool DrawableBoard::bottomPlayerToMove() const
{
    return totalHalfmoves % 2 == !bottomIsWhite;
}

std::optional<StandardMove> DrawableBoard::getLastMovePlayed() const
{
    if (gameMoves.empty()) {
        return std::nullopt;
    }

    return gameMoves.back();
}

void DrawableBoard::reset(bool whiteOnBottom)
{
    bottomIsWhite = whiteOnBottom;
    loadStartingPosition();
    initGraphicalMembers();
}

// BOARD METHODS
void DrawableBoard::initGraphicalMembers()
{
    // Reset current members
    currentlySelected.reset();
    hoveringPeice.setTexture(peiceTextures[0]);
    hiddenSquare = -1;

    // initialize zobrist hash and set sprites for all of the peices and squares
    for (int i = 0; i < 64; ++i) {
        int file = i % 8;
        int rank = i / 8;
        squarePositions[i] = boardPosition + (bottomIsWhite ? sf::Vector2f((float)(file * 120), (float)((7 - rank) * 120)) : sf::Vector2f((float)((7 - file) * 120), (float)(rank * 120)));
    }
}

void DrawableBoard::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    // Draw checkerboard
    sf::RectangleShape square(sf::Vector2f(120, 120));
    for (int i = 0; i < 64; ++i) {
        square.setPosition(squarePositions[i]);
        square.setFillColor(IS_LIGHT_SQUARE(i) ? LIGHT_SQUARE_COLOR : DARK_SQUARE_COLOR);
        target.draw(square);
    }
    // Add highlights where needed
    if (!gameMoves.empty()) {
        int s = gameMoves.back().startSquare;
        int t = gameMoves.back().targetSquare;
        square.setPosition(squarePositions[s]);
        square.setFillColor(IS_LIGHT_SQUARE(s) ? LIGHT_PREVIOUS_MOVE : DARK_PREVIOUS_MOVE);
        target.draw(square);
        square.setPosition(squarePositions[t]);
        square.setFillColor(IS_LIGHT_SQUARE(t) ? LIGHT_PREVIOUS_MOVE : DARK_PREVIOUS_MOVE);
        target.draw(square);
    }
    if (currentlySelected.has_value()) {
        int s = currentlySelected.value();
        square.setPosition(squarePositions[s]);
        square.setFillColor(IS_LIGHT_SQUARE(s) ? LIGHT_CURRENTLY_SELECTED : DARK_CURRENTLY_SELECTED);
        target.draw(square);

        for (const StandardMove& move : currentLegalMoves) {
            if (move.startSquare == s) {
                int t = move.targetSquare;
                square.setPosition(squarePositions[t]);
                square.setFillColor(IS_LIGHT_SQUARE(t) ? LIGHT_AVAILABLE_TARGET : DARK_AVAILABLE_TARGET);
                target.draw(square);
            }
        }
    }

    // Draw peices
    sf::Sprite peice;
    for (int i = 0; i < 64; ++i) {
        if (i == hiddenSquare) {
            continue;
        }
        peice.setPosition(squarePositions[i]);
        peice.setTexture(peiceTextures[peices[i]]);
        target.draw(peice);
    }

    target.draw(hoveringPeice);
}