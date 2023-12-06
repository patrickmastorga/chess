#include <SFML/Graphics.hpp>
#include <SFML/Graphics/Font.hpp>
#include <chrono>
#include <optional>
#include "DrawableBoard.hpp"
#include "../chess.hpp"
#include "../v2/engine_v2.hpp"

#define TITLE_HEIGHT 40.0f
#define DISTANCE_FROM_TOP 3.0f
#define TITLE_SIZE 26U
#define SUBTITLE_SIZE 12U

#define ENGINE_NAME "engine_v2"


/*
THINGS THAT NEED FIXING
 - Threefold repitition
 - Promotion
*/

void drawTitle(sf::RenderTarget& target, bool whiteOnBottom, std::optional<int> gameOver=std::nullopt) {
    // Title bar settings
    sf::RectangleShape titleBar(sf::Vector2f(target.getSize().x, TITLE_HEIGHT));
    titleBar.setFillColor(sf::Color(40, 40, 40)); // Dark grey color

    // Load font
    sf::Font font;
    font.loadFromFile("assets/fonts/arial.ttf");

    // Middle text
    sf::Text middle("  vs.  ", font, TITLE_SIZE);
    middle.setFillColor(sf::Color::White);

    // Calculate position
    float textWidth = middle.getGlobalBounds().width;
    float windowWidth = static_cast<float>(target.getSize().x);
    sf::Vector2f titlePos((windowWidth - textWidth) / 2.0f, DISTANCE_FROM_TOP);
    middle.setPosition(titlePos);

    // Calculate text color
    auto leftColor = sf::Color::White;
    auto rightColor = sf::Color::White;
    if (gameOver.has_value()) {
        if (gameOver.value() == 0) {
            leftColor = sf::Color::Yellow;
            rightColor = sf::Color::Yellow;
        
        } else {
            leftColor = gameOver.value() > 0 ? sf::Color::Green : sf::Color::Red;
            rightColor = gameOver.value() < 0 ? sf::Color::Green : sf::Color::Red;
        }
    }

    // White player
    sf::Text left(whiteOnBottom ? "human" : ENGINE_NAME, font, TITLE_SIZE);
    left.setOrigin(left.getGlobalBounds().width, 0.0f);
    left.setFillColor(leftColor);
    left.setPosition(titlePos);

    // Black player
    sf::Text right(!whiteOnBottom ? "human" : ENGINE_NAME, font, TITLE_SIZE);
    right.setOrigin(-middle.getGlobalBounds().width, 0.0f);
    right.setFillColor(rightColor);
    right.setPosition(titlePos);

    // Controls
    sf::Text controls1("reset (enter)", font, TITLE_SIZE / 2);
    sf::Text controls2("switch (tab)", font, TITLE_SIZE / 2);
    controls1.setPosition(2.0f, DISTANCE_FROM_TOP / 2.0f);
    controls2.setPosition(2.0f, TITLE_HEIGHT / 2.0f + DISTANCE_FROM_TOP / 2.0f);
    controls1.setFillColor(sf::Color(150, 150, 150));
    controls2.setFillColor(sf::Color(150, 150, 150));
    

    target.draw(titleBar);
    target.draw(middle);
    target.draw(left);
    target.draw(right);
    target.draw(controls1);
    target.draw(controls2);
}

int main()
{
    sf::RenderWindow window(sf::VideoMode(960, 960 + TITLE_HEIGHT), "chessgui", sf::Style::Close | sf::Style::Titlebar);
    window.setFramerateLimit(60);
    
    sf::Vector2f boardPosition(0, TITLE_HEIGHT);
    bool whiteOnBottom = true;
    bool gameOver = false;
    bool mouseHold = false;

    DrawableBoard board(boardPosition, whiteOnBottom);
    EngineV2 engine;

    while (window.isOpen())
    {
        // Handle events
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed) {
                window.close();
                return 0;
            }

            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Enter) {
                    // Reset game
                    board.reset(whiteOnBottom);
                    engine.loadStartingPosition();
                    gameOver = false;
                    mouseHold = false;
                }

                if (event.key.code == sf::Keyboard::Tab) {
                    // Reset game and switch sides
                    whiteOnBottom = !whiteOnBottom;
                    board.reset(whiteOnBottom);
                    engine.loadStartingPosition();
                    gameOver = false;
                    mouseHold = false;
                }
            }
        }


        if (gameOver) {
            continue;
        }

        // Update screen
        drawTitle(window, whiteOnBottom);
        window.draw(board);
        window.display();

        if (board.bottomPlayerToMove()) {
            // PLAYER TURN
            // Look for player input
            if (sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
                sf::Vector2i mousePos = sf::Mouse::getPosition(window);
                if (mouseHold) {
                    // Mouse is being held
                    board.mouseDrag(sf::Vector2f((float)mousePos.x, (float)mousePos.y));
                } else {
                    // Mousedown
                    board.mouseDown(sf::Vector2f((float)mousePos.x, (float)mousePos.y));
                    mouseHold = true;
                }
            
            } else if (mouseHold) {
                sf::Vector2i mousePos = sf::Mouse::getPosition(window);
                // Mouseup
                board.mouseUp(sf::Vector2f((float)mousePos.x, (float)mousePos.y));
                mouseHold = false;
            }

        } else {
            // COMPUTER TURN
            // Check if player move has ended game
            if (board.gameOver().has_value()) {
                gameOver = true;
                drawTitle(window, whiteOnBottom, board.gameOver());
                window.display();
                continue;
            }

            // Update engine position with player input
            if (board.getLastMovePlayed().has_value()) {
                engine.inputMove(board.getLastMovePlayed().value());
            }
            
            // Get computer move
            using namespace std::chrono_literals;
            StandardMove computerMove = engine.computerMove(1000ms);

            // Update board and engine position with computer move
            board.inputMove(computerMove);
            engine.inputMove(computerMove);

            // Check if computer move has ended game
            if (board.gameOver().has_value()) {
                gameOver = true;
                drawTitle(window, whiteOnBottom, board.gameOver());
                window.display();
                continue;
            }

            mouseHold = false;
        }
        
    }
    
    return 0;
}