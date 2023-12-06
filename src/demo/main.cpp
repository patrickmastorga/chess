#include <SFML/Graphics.hpp>
#include <SFML/Graphics/Font.hpp>
#include <chrono>
#include <string>
#include "DrawableBoard.hpp"
#include "../chess.hpp"
#include "../v2/engine_v2.hpp"

void drawTitle(sf::RenderTarget& target, int width, const std::string &text) {
    // Title bar settings
    sf::RectangleShape titleBar(sf::Vector2f(target.getSize().x, width));
    titleBar.setFillColor(sf::Color(40, 40, 40)); // Dark grey color

    // Use the default font
    sf::Font font;
    font.loadFromFile();

    sf::Text titleText("Your Title Here", font, 18);
    titleText.setFillColor(sf::Color::White);

    // Calculate the position to center the text horizontally
    float textWidth = titleText.getGlobalBounds().width;
    float windowWidth = static_cast<float>(target.getSize().x);
    titleText.setPosition((windowWidth - textWidth) / 2.0f, 5);
}

int main()
{
    sf::RenderWindow window(sf::VideoMode(960, 960), "chessgui", sf::Style::Close | sf::Style::Titlebar);
    auto desktop = sf::VideoMode::getDesktopMode();
    window.setPosition(sf::Vector2i(desktop.width/2 - window.getSize().x/2, desktop.height/2 - window.getSize().y/2 - 50));
    
    int titleWidth = 40;
    sf::Vector2f boardPosition(0, titleWidth);
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
        window.clear();
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
                continue;
            }

            mouseHold = false;
        }
        
    }
    
    return 0;
}