#include <iostream>

#include "perft.h"
#include "EngineV1_1.h"
#include "StandardMove.h"
#include "Game.h"

using namespace std;
using namespace std::chrono_literals;

int main()
{
	Game game;

	//engine.inputMove(StandardMove(52, 45));

	//perft::testSearchEfficiency(engine, 6, 25);

	game.inputMove(StandardMove(12, 28));
	game.inputMove(StandardMove(62, 45));
	game.inputMove(StandardMove(1 , 18));
	game.inputMove(StandardMove(57, 42));
	game.inputMove(StandardMove(6 , 21));
	game.inputMove(StandardMove(51, 35));
	game.inputMove(StandardMove(28, 35));
	game.inputMove(StandardMove(45, 35));
	game.inputMove(StandardMove(18, 35));
	game.inputMove(StandardMove(59, 35));
	game.inputMove(StandardMove(11, 27));
	game.inputMove(StandardMove(58, 30));
	game.inputMove(StandardMove(2 , 20));
	game.inputMove(StandardMove(52, 36));
	game.inputMove(StandardMove(21, 36));
	game.inputMove(StandardMove(30, 3 ));
	game.inputMove(StandardMove(0 , 3 ));

	game.inputMove(StandardMove(35, 8));

	cout << game.asPGN();
	cout << game.asFEN();

	auto moves = game.getLegalMoves();

	cout << "DONE";
	
	return 0;
}