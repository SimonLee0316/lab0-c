#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "mcts.h"
#include "util.h"

struct node {
    int move;
    char player;
    int n_visits;
    uint64_t score;
    struct node *parent;
    struct node *children[N_GRIDS];
};

static struct node *new_node(int move, char player, struct node *parent)
{
    struct node *node = malloc(sizeof(struct node));
    node->move = move;
    node->player = player;
    node->n_visits = 0;
    node->score = 0;
    node->parent = parent;
    memset(node->children, 0, sizeof(node->children));
    return node;
}

static void free_node(struct node *node)
{
    for (int i = 0; i < N_GRIDS; i++)
        if (node->children[i])
            free_node(node->children[i]);
    free(node);
}

static uint64_t fixed_power_int(uint64_t x,
                                unsigned int frac_bits,
                                unsigned int n)
{
    uint64_t result = 1ULL << frac_bits;

    if (n) {
        for (;;) {
            if (n & 1) {
                result *= x;
                result += 1ULL << (frac_bits - 1);
                result >>= frac_bits;
            }
            n >>= 1;
            if (!n)
                break;
            x *= x;
            x += 1ULL << (frac_bits - 1);
            x >>= frac_bits;
        }
    }

    return result;
}

uint64_t do_log(uint64_t num)
{
    uint64_t result = 0ULL;

    if (num == 0)
        return ULLONG_MAX;
    if (num == 1)
        return 0ULL;

    for (int i = 1; i <= 16; i++) {
        if (i % 2 == 0) {
            result -= fixed_power_int(num << SHIFT_AMOUNT, SHIFT_AMOUNT, i) / i;
        } else {
            result += fixed_power_int(num << SHIFT_AMOUNT, SHIFT_AMOUNT, i) / i;
        }
    }
    return result;
}

uint64_t do_sqrt(uint64_t n)
{
    if (n <= 1)
        return n;
    uint64_t x = n;
    uint64_t y = 0ULL;
    uint64_t epsilon = 1ULL << (SHIFT_AMOUNT - 2);

    while (x - y > epsilon) {
        uint64_t mid = y + (x - y) / 2;
        uint64_t square = (mid * mid) >> SHIFT_AMOUNT;

        if (square < x) {
            y = mid;
        } else {
            x = mid;
        }
    }
    return x;
}

static inline uint64_t fix_uct_score(uint64_t n_total,
                                     uint64_t n_visits,
                                     uint64_t score)
{
    if (n_visits == 0) {
        return UINT64_MAX;
    }
    uint64_t result = (score / ((n_visits) << SHIFT_AMOUNT)) << SHIFT_AMOUNT;
    uint64_t tmp = do_sqrt((do_log(n_total) / ((n_visits) << SHIFT_AMOUNT))
                           << SHIFT_AMOUNT);
    tmp *= do_sqrt(EXPLORATION_FACTOR);
    tmp >>= SHIFT_AMOUNT;

    return (result + tmp) > result ? (result + tmp) : UINT64_MAX;
}

static struct node *select_move(struct node *node)
{
    struct node *best_node = NULL;
    uint64_t best_score = 0ULL;
    for (int i = 0; i < N_GRIDS; i++) {
        if (!node->children[i])
            continue;
        uint64_t score =
            fix_uct_score(node->n_visits, node->children[i]->n_visits,
                          node->children[i]->score);
        if (score > best_score) {
            best_score = score;
            best_node = node->children[i];
        }
    }
    return best_node;
}

static uint64_t simulate(char *table, char player)
{
    char win;
    char current_player = player;
    char temp_table[N_GRIDS];
    memcpy(temp_table, table, N_GRIDS);
    while (1) {
        int *moves = available_moves(temp_table);
        if (moves[0] == -1) {
            free(moves);
            break;
        }
        int n_moves = 0;
        while (n_moves < N_GRIDS && moves[n_moves] != -1)
            ++n_moves;
        int move = moves[rand() % n_moves];
        free(moves);
        temp_table[move] = current_player;
        if ((win = check_win(temp_table)) != ' ')
            return calculate_win_value(win, player);
        current_player ^= 'O' ^ 'X';
    }
    return (uint64_t) (1ULL << (SHIFT_AMOUNT - 1));
}

static void backpropagate(struct node *node, uint64_t score)
{
    while (node) {
        node->n_visits++;
        node->score += score;
        node = node->parent;
        score = 1 - score;
    }
}

static void expand(struct node *node, char *table)
{
    int *moves = available_moves(table);
    int n_moves = 0;
    while (n_moves < N_GRIDS && moves[n_moves] != -1)
        ++n_moves;
    for (int i = 0; i < n_moves; i++) {
        node->children[i] = new_node(moves[i], node->player ^ 'O' ^ 'X', node);
    }
    free(moves);
}

int mcts(char *table, char player)
{
    char win;
    struct node *root = new_node(-1, player, NULL);
    for (int i = 0; i < ITERATIONS; i++) {
        struct node *node = root;
        char temp_table[N_GRIDS];
        memcpy(temp_table, table, N_GRIDS);
        while (1) {
            if ((win = check_win(temp_table)) != ' ') {
                uint64_t score =
                    calculate_win_value(win, node->player ^ 'O' ^ 'X');
                backpropagate(node, score);
                break;
            }
            if (node->n_visits == 0) {
                uint64_t score = simulate(temp_table, node->player);
                backpropagate(node, score);
                break;
            }
            if (node->children[0] == NULL)
                expand(node, temp_table);
            node = select_move(node);
            assert(node);
            temp_table[node->move] = node->player ^ 'O' ^ 'X';
        }
    }
    struct node *best_node = NULL;
    int most_visits = -1;
    for (int i = 0; i < N_GRIDS; i++) {
        if (root->children[i] && root->children[i]->n_visits > most_visits) {
            most_visits = root->children[i]->n_visits;
            best_node = root->children[i];
        }
    }
    int best_move = best_node->move;
    free_node(root);
    return best_move;
}
