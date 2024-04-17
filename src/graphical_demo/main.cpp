//////////////////////////////////////////////////////////
// EDIT THESE VARIABLES
//////////////////////////////////////////////////////////
#define STANDARD_ENGINE_HEADER_FILE "EngineV1_3.h"
#define STANDARD_ENGINE_CLASS_NAME EngineV1_3
#define ENGINE_NAME "engine_v1.3"
#define HUMAN_NAME "human"
#define THINK_TIME 200ms
#define BOARD_SIZE 960
#define LOG_FILE_NAME "log.txt"
//////////////////////////////////////////////////////////

#include <SFML/Graphics.hpp>
#include <SFML/Graphics/Font.hpp>
#include <chrono>
#include <optional>
#include <string>
#include <fstream>
#include <map>

#include STANDARD_ENGINE_HEADER_FILE
#include "DrawableBoard.h"
#include "StandardMove.h"

#define TITLE_HEIGHT 40.0f
#define DISTANCE_FROM_TOP 3.0f
#define TITLE_SIZE 26U
#define SUBTITLE_SIZE 12U


const sf::Vector2f gameSize(960.0f, 960.0f + TITLE_HEIGHT);

/*
THINGS THAT NEED FIXING
 - Threefold repitition causes exeption
*/

static void drawTitle(sf::RenderTarget& target, bool whiteOnBottom, std::optional<int> gameOver = std::nullopt) {
    // Title bar settings
    sf::RectangleShape titleBar(sf::Vector2f(gameSize.x, TITLE_HEIGHT));
    titleBar.setFillColor(sf::Color(40, 40, 40)); // Dark grey color

    // Load font
    sf::Font font;
    font.loadFromFile("assets/fonts/arial.ttf");

    // Middle text
    sf::Text middle("  vs.  ", font, TITLE_SIZE);
    middle.setFillColor(sf::Color::White);

    // Calculate position
    float textWidth = middle.getGlobalBounds().width;
    sf::Vector2f titlePos((gameSize.x - textWidth) / 2.0f, DISTANCE_FROM_TOP);
    middle.setPosition(titlePos);

    // Calculate text color
    auto leftColor = sf::Color::White;
    auto rightColor = sf::Color::White;
    if (gameOver.has_value()) {
        if (gameOver.value() == 0) {
            leftColor = sf::Color::Yellow;
            rightColor = sf::Color::Yellow;

        }
        else {
            leftColor = gameOver.value() > 0 ? sf::Color::Green : sf::Color::Red;
            rightColor = gameOver.value() < 0 ? sf::Color::Green : sf::Color::Red;
        }
    }

    // White player
    sf::Text left(whiteOnBottom ? HUMAN_NAME : ENGINE_NAME, font, TITLE_SIZE);
    left.setOrigin(left.getGlobalBounds().width, 0.0f);
    left.setFillColor(leftColor);
    left.setPosition(titlePos);

    // Black player
    sf::Text right(!whiteOnBottom ? HUMAN_NAME : ENGINE_NAME, font, TITLE_SIZE);
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
    // SFML window creation
    sf::Vector2u windowSize(gameSize * (BOARD_SIZE / 960.0f));
    sf::RenderWindow window(sf::VideoMode(windowSize.x, windowSize.y), "demo", sf::Style::Close | sf::Style::Titlebar);
    window.setVerticalSyncEnabled(true);
    sf::Image icon;
    icon.loadFromFile("assets/120px/icon.png");
    window.setIcon(icon.getSize().x, icon.getSize().y, icon.getPixelsPtr());

    sf::View gameView(gameSize / 2.0f, gameSize);

    sf::Vector2f boardPosition(0, TITLE_HEIGHT);
    bool whiteOnBottom = true;
    std::optional<int> gameOver = std::nullopt;
    bool mouseHold = false;

    DrawableBoard board(boardPosition, whiteOnBottom);
    STANDARD_ENGINE_CLASS_NAME engine;

    std::ofstream game_log(LOG_FILE_NAME);

    std::map<std::string, std::string> headers;
    headers["White"] = whiteOnBottom ? HUMAN_NAME : ENGINE_NAME;
    headers["Black"] = whiteOnBottom ? ENGINE_NAME : HUMAN_NAME;
    int gameNumber = 1;


    while (window.isOpen())
    {
        // Handle events
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed) {
                if (board.getLastMovePlayed().has_value() && !gameOver.has_value() && game_log.is_open()) {
                    headers["Event"] = "Graphical Demo Game " + std::to_string(gameNumber++);
                    headers["Termination"] = "Forfiet";
                    game_log << board.asPGN(headers);
                }
                if (game_log.is_open()) {
                    game_log.close();
                }
                window.close();
                return 0;
            }

            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Enter) {
                    // Forfiet
                    if (board.getLastMovePlayed().has_value() && !gameOver.has_value() && game_log.is_open()) {
                        headers["Event"] = "Graphical Demo Game " + std::to_string(gameNumber++);
                        headers["Termination"] = "Forfiet";
                        game_log << board.asPGN(headers);
                    }
                    // Reset game
                    board.reset(whiteOnBottom);
                    engine.loadStartingPosition();
                    gameOver = std::nullopt;
                    mouseHold = false;
                }

                if (event.key.code == sf::Keyboard::Tab) {
                    // Forfiet
                    if (board.getLastMovePlayed().has_value() && !gameOver.has_value() && game_log.is_open()) {
                        headers["Event"] = "Graphical Demo Game " + std::to_string(gameNumber++);
                        headers["Termination"] = "Forfiet";
                        game_log << board.asPGN(headers);
                        headers["White"] = whiteOnBottom ? HUMAN_NAME : ENGINE_NAME;
                        headers["Black"] = whiteOnBottom ? ENGINE_NAME : HUMAN_NAME;
                    }
                    // Reset game and switch sides
                    whiteOnBottom = !whiteOnBottom;
                    board.reset(whiteOnBottom);
                    engine.loadStartingPosition();
                    gameOver = std::nullopt;
                    mouseHold = false;
                }
            }
        }

        // Update screen
        window.setView(gameView);
        drawTitle(window, whiteOnBottom, gameOver);
        window.draw(board);
        window.display();

        if (gameOver.has_value()) {
            continue;
        }

        if (board.bottomPlayerToMove()) {
            // PLAYER TURN
            // Look for player input
            sf::Vector2i windowMousePos = sf::Mouse::getPosition(window);
            sf::Vector2f viewMousePos = window.mapPixelToCoords(windowMousePos);
            if (sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
                if (mouseHold) {
                    // Mouse is being held
                    board.mouseDrag(viewMousePos);
                }
                else {
                    // Mousedown
                    board.mouseDown(viewMousePos);
                    mouseHold = true;
                }

            }
            else if (mouseHold) {
                // Mouseup
                board.mouseUp(viewMousePos);
                mouseHold = false;
            }

        }
        else {
            // Update engine position with player input
            if (board.getLastMovePlayed().has_value()) {
                engine.inputMove(board.getLastMovePlayed().value());
            }

            // COMPUTER TURN
            // Check if player move has ended game
            if (board.gameOver().has_value()) {
                if (game_log.is_open()) {
                    headers["Event"] = "Graphical Demo Game " + std::to_string(gameNumber++);
                    headers["Termination"] = "Normal";
                    game_log << board.asPGN(headers);
                }
                gameOver = board.gameOver();
                continue;
            }

            // Get computer move
            using namespace std::chrono_literals;
            StandardMove computerMove = engine.computerMove(THINK_TIME);

            // Update board and engine position with computer move
            board.inputMove(computerMove);
            engine.inputMove(computerMove);

            // Check if computer move has ended game
            if (board.gameOver().has_value()) {
                if (game_log.is_open()) {
                    headers["Event"] = "Graphical Demo Game " + std::to_string(gameNumber++);
                    headers["Termination"] = "Normal";
                    game_log << board.asPGN(headers);
                }
                gameOver = board.gameOver();
                continue;
            }

            mouseHold = false;
        }

    }

    return 0;
}