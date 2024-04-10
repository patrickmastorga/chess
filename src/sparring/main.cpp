//////////////////////////////////////////////////////////
// EDIT THESE VARIABLES
//////////////////////////////////////////////////////////
#define ENGINE_1_HEADER_FILE "EngineV1_1.h"
#define ENGINE_1_CLASS_NAME EngineV1_1
#define ENGINE_1_NAME "engine_v1.1"
#define ENGINE_2_HEADER_FILE "EngineV1_1.h"
#define ENGINE_2_CLASS_NAME EngineV1_1
#define ENGINE_2_NAME "engine_v1.1"
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

#include ENGINE_1_HEADER_FILE
#include ENGINE_1_HEADER_FILE
#include "DrawableBoard.h"
#include "StandardMove.h"

#define TITLE_HEIGHT 40.0f
const sf::Vector2f gameSize(960.0f, 960.0f + TITLE_HEIGHT);

static void drawTitle(sf::RenderTarget& target, std::string white, std::string black) {
    static constexpr float DISTANCE_FROM_TOP = 3.0f;
    static constexpr unsigned int TITLE_SIZE = 26U;

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

    // White player
    sf::Text left(white, font, TITLE_SIZE);
    left.setOrigin(left.getGlobalBounds().width, 0.0f);
    left.setPosition(titlePos);

    // Black player
    sf::Text right(black, font, TITLE_SIZE);
    right.setOrigin(-middle.getGlobalBounds().width, 0.0f);
    right.setPosition(titlePos);

    // Sparring text
    sf::Text title("SPARRING", font, TITLE_SIZE);
    title.setPosition(DISTANCE_FROM_TOP, DISTANCE_FROM_TOP);

    target.draw(titleBar);
    target.draw(middle);
    target.draw(left);
    target.draw(right);
    target.draw(title);
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
    
    // Initialize board
    DrawableBoard board(sf::Vector2f(0, TITLE_HEIGHT), true);

    // Initialize engines
    ENGINE_1_CLASS_NAME engine1;
    ENGINE_2_CLASS_NAME engine2;

    // Setup for loggin results
    std::ofstream game_log(LOG_FILE_NAME);
    std::map<std::string, std::string> headers;

    // Loop
    std::ifstream positions("sparring_positions.txt");

    std::string fen;
    while (std::getline(positions, fen)) {
        using namespace std::chrono_literals;

        // Play game
        board.loadFEN(fen);
        engine1.loadFEN(fen);
        engine2.loadFEN(fen);

        while (!board.gameOver().has_value()) {

            // Check if window closed
            sf::Event event;
            while (window.pollEvent(event))
            {
                if (event.type == sf::Event::Closed) {
                    if (game_log.is_open()) {
                        game_log.close();
                    }
                    window.close();
                    return 0;
                }
            }

            // Update screen
            window.setView(gameView);
            drawTitle(window, ENGINE_1_NAME, ENGINE_2_NAME);
            window.draw(board);
            window.display();

            // Get engine 1's move
            StandardMove computerMove = engine1.computerMove(THINK_TIME);
            board.inputMove(computerMove);
            engine1.inputMove(computerMove);
            engine2.inputMove(computerMove);

            if (board.gameOver().has_value()) {
                break;
            }

            // Update screen
            window.setView(gameView);
            drawTitle(window, ENGINE_1_NAME, ENGINE_2_NAME);
            window.draw(board);
            window.display();

            // Get engine 2's move
            computerMove = engine2.computerMove(THINK_TIME);
            board.inputMove(computerMove);
            engine1.inputMove(computerMove);
            engine2.inputMove(computerMove);
        }

        // Log results
        headers["White"] = ENGINE_1_NAME;
        headers["Black"] = ENGINE_2_NAME;
        headers["Event"] = "Sparring Game FEN: " + fen;
        headers["Termination"] = "Normal";
        game_log << board.asPGN(headers);


        // Play game (colors swapped)
        board.loadFEN(fen);
        engine1.loadFEN(fen);
        engine2.loadFEN(fen);

        while (!board.gameOver().has_value()) {

            // Check if window closed
            sf::Event event;
            while (window.pollEvent(event))
            {
                if (event.type == sf::Event::Closed) {
                    if (game_log.is_open()) {
                        game_log.close();
                    }
                    window.close();
                    return 0;
                }
            }

            // Update screen
            window.setView(gameView);
            drawTitle(window, ENGINE_2_NAME, ENGINE_1_NAME);
            window.draw(board);
            window.display();

            // Get engine 2's move
            StandardMove computerMove = engine2.computerMove(THINK_TIME);
            board.inputMove(computerMove);
            engine1.inputMove(computerMove);
            engine2.inputMove(computerMove);

            if (board.gameOver().has_value()) {
                break;
            }

            // Update screen
            window.setView(gameView);
            drawTitle(window, ENGINE_2_NAME, ENGINE_1_NAME);
            window.draw(board);
            window.display();

            // Get engine 1's move
            computerMove = engine1.computerMove(THINK_TIME);
            board.inputMove(computerMove);
            engine1.inputMove(computerMove);
            engine2.inputMove(computerMove);
        }

        // Log results
        headers["White"] = ENGINE_1_NAME;
        headers["Black"] = ENGINE_2_NAME;
        headers["Event"] = "Sparring Game FEN: " + fen;
        headers["Termination"] = "Normal";
        game_log << board.asPGN(headers);
    }

    // End of program
    if (game_log.is_open()) {
        game_log.close();
    }
    window.close();
    return 0;
}