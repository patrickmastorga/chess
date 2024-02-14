## Chess Engine
Chess Engine written in C++ from scratch


# Move generation
Use a hybrid of pseudo legal and legal move generation
 - Code always checks if king is in check and if any peices sre pinned before generating moves
 - King moves are pseudo legal
 - Movement of pinned peices are pseudo legal
 - All other moves are guarenteed to be legal
In the future, I might implement bitboard representations of the board to speed up move generation


# Search Algorithm
Minimax search algorithm
Features I have implemented:
 - Quiscence search
 - Iterative deepening
 - Alpha-Beta pruning
 - Move ordering

Features I plan to implement:
 - Transposition table
 - PV-search (null window search)
 - Killer moves
 - Search extensions
 - Late move reduction


# Static Evaluation
Uses simple earlygame/endgame peice-square tables

Upgrades I have planned:
 - King safety
 - Mobility
 - Pawn structure
I plan on expirimenting with using a neural network for static evaluation