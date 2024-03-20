#pragma once

#define ITERATIONS 100000

#define EXPLORATION_FACTOR 1ULL << (SHIFT_AMOUNT + 1)

int mcts(char *table, char player);
