#include <iostream>

//#include "perft.h"
#include "EngineV1_3.h"
#include "StandardMove.h"
//#include "Game.h"


using namespace std;
//using namespace std::chrono_literals;

int main()
{
	EngineV1_3 engine;

	cout << "Starting posiiton: ";

	cout << engine.testEval() << endl;

	engine.inputMove(StandardMove(12, 28));
	engine.inputMove(StandardMove(52, 36));
	engine.inputMove(StandardMove(6, 21));
	engine.inputMove(StandardMove(57, 42));

	cout << "After moves: ";

	cout << engine.testEval() << endl;

	engine.loadFEN("r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3");

	cout << "From fen: ";

	cout << engine.testEval() << endl;
	
	return 0;
}