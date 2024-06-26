#include <assert.h>
#include <float.h>
#include <math.h>
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
    double score;
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

static __uint64_t do_log(__uint64_t x)
{
    __uint64_t x_plus_1 = x + (1U << SHIFT_AMOUNT);
    __uint64_t x_minus_1 = x - (1U << SHIFT_AMOUNT);
    __uint64_t tmp1 = (x_minus_1 << SHIFT_AMOUNT) / x_plus_1;
    __uint64_t tmp2 = (tmp1 * tmp1) >> SHIFT_AMOUNT;
    __uint64_t result = 1U << SHIFT_AMOUNT;
    for (__uint64_t i = 1; i < 10; i++) {
        __uint64_t term =
            (tmp2 << SHIFT_AMOUNT) / ((2 * i + 1) << SHIFT_AMOUNT);
        result += term;
        tmp2 = (tmp2 * tmp2) >> SHIFT_AMOUNT;
    }

    result <<= 1;
    result = (result * tmp1) >> SHIFT_AMOUNT;
    return result;
}

static __uint64_t do_sqrt(__uint64_t x)
{
    if (x <= 1)
        return x;

    __uint64_t high = x;
    __uint64_t low = 0UL;
    __uint64_t epsilon = 1UL << 10;
    while ((high - low) > epsilon) {
        __uint64_t mid = (low + high) / 2;
        __uint64_t square = (mid * mid) >> 12;
        if (square == x)
            return mid;
        else if (square < x)
            low = mid;
        else
            high = mid;
    }
    return (low + high) / 2;
}

static inline double uct_score(__uint64_t n_total,
                               __uint64_t n_visits,
                               double score)
{
    // printf("all %lu,visit %lu,score %lf\n",n_total,n_visits,score);
    if (n_visits == 0)
        return DBL_MAX;
    __uint64_t fix_n_total = n_total * FIX_POINT_BIAS;
    __uint64_t fix_n_visits = n_visits * FIX_POINT_BIAS;
    __uint64_t fix_score = score * FIX_POINT_BIAS;
    __uint64_t win = (fix_score * FIX_POINT_BIAS) / fix_n_visits;
    __uint64_t tmpright1 =
        (do_log(fix_n_total) * FIX_POINT_BIAS) / fix_n_visits;
    __uint64_t tmpright2 = do_sqrt(tmpright1);
    __uint64_t tmpleft = do_log(EXPLORATION_FACTOR * FIX_POINT_BIAS);
    __uint64_t deep = (tmpright2 * tmpleft) / FIX_POINT_BIAS;
    // printf("win :%lu tmpright1: %lu tmpright2: %lu tmpleft:
    // %lu\n",win,tmpright1,tmpright2,tmpleft);
    win += deep;
    return (double) win / FIX_POINT_BIAS;
}

static struct node *select_move(struct node *node)
{
    struct node *best_node = NULL;
    double best_score = -1;
    for (int i = 0; i < N_GRIDS; i++) {
        if (!node->children[i])
            continue;
        double score = uct_score(node->n_visits, node->children[i]->n_visits,
                                 node->children[i]->score);
        // printf("score : %f\n",score);
        if (score > best_score) {
            best_score = score;
            best_node = node->children[i];
        }
    }
    return best_node;
}

static double simulate(char *table, char player)
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
    return 0.5;
}

static void backpropagate(struct node *node, double score)
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
                double score =
                    calculate_win_value(win, node->player ^ 'O' ^ 'X');
                backpropagate(node, score);
                break;
            }
            if (node->n_visits == 0) {
                double score = simulate(temp_table, node->player);
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
