#include <iostream>

#include "perft.h"
#include "EngineV1_1.h"
#include "StandardMove.h"
#include "Game.h"

using namespace std;
using namespace std::chrono_literals;

int main()
{
	EngineV1_1 engine("Q7/8/8/5K2/8/7P/2Q5/5k2 w - - 3 68");

	cout << engine.computerMove(100ms);
	
	return 0;
}