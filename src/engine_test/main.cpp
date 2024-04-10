#include <iostream>

#include "perft.h"
#include "EngineV1_1.h"
#include "StandardMove.h"
#include "Game.h"

using namespace std;
using namespace std::chrono_literals;

int main()
{
	EngineV1_1 engine("r5k1/5pp1/4p2p/1P1b4/r7/2Q5/2R3PP/6K1 b - - 1 35");

	engine.inputMove(engine.computerMove(150ms));
	engine.inputMove(engine.computerMove(150ms));
	
	return 0;
}