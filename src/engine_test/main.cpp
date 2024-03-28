#include <iostream>

#include "perft.h"
#include "EngineV1_1.h"
#include "StandardMove.h"

using namespace std;
using namespace std::chrono_literals;

int main()
{
	EngineV1_1 engine;

	//engine.inputMove(StandardMove(52, 45));

	perft::testSearchEfficiency(engine, 6, 25);
	
	return 0;
}