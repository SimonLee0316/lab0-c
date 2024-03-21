#pragma once

#define ITERATIONS 100000

#define EXPLORATION_FACTOR 2.0

#define SHIFT_AMOUNT 16
#define FIX_POINT_BIAS (1 << SHIFT_AMOUNT)

int mcts(char *table, char player);
