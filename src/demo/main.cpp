#include <SFML/Graphics.hpp>
#include <chrono>
#include "DrawableBoard.hpp"
#include "../chess.hpp"
#include "../v2/engine_v2.hpp"

int main()
{
    sf::RenderWindow window(sf::VideoMode(960, 960), "chessgui", sf::Style::Close | sf::Style::Titlebar);
    auto desktop = sf::VideoMode::getDesktopMode();
    window.setPosition(sf::Vector2i(desktop.width/2 - window.getSize().x/2, desktop.height/2 - window.getSize().y/2 - 50));
    
    DrawableBoard board(sf::Vector2f(0, 0), true);

    EngineV2 engine;

    bool mouseHold = false;

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
        }

        // Update screen
        window.clear();
        window.draw(board);
        window.display();

        // Mouse Input
        if (board.bottomPlayerToMove()) {
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
            // Update engine position with player input
            if (board.getLastMovePlayed().has_value()) {
                engine.inputMove(board.getLastMovePlayed().value());
            }
            
            // Get computer move
            using namespace std::chrono_literals;
            StandardMove computerMove = engine.computerMove(1000s);

            // Update board and engine position with computer move
            board.inputMove(computerMove);
            engine.inputMove(computerMove);

            mouseHold = false;
        }
        
    }
    
    return 0;
}