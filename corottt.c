#include "corottt.h"

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"

#include "agents/mcts.h"
#include "agents/negamax.h"
#include "game.h"

#if defined(__APPLE__)
#include <mach/mach_time.h>
#else /* Assume POSIX environments */
#include <time.h>
#endif

static int move_record[N_GRIDS];
static int move_count = 0;

struct task {
    jmp_buf env;
    struct list_head list;
    char task_name[30];
};

struct arg {
    char *task_name;
};

static LIST_HEAD(tasklist);
static void (**tasks)(void *);
static struct arg *args;
static int ntasks;
static jmp_buf sched;
static struct task *cur_task;
static bool roundend = false;
static int rounds;
static char table[N_GRIDS];

static void record_move(int move)
{
    move_record[move_count++] = move;
}

static void print_moves()
{
    printf("Moves: ");
    for (int i = 0; i < move_count; i++) {
        printf("%c%d", 'A' + GET_COL(move_record[i]),
               1 + GET_ROW(move_record[i]));
        if (i < move_count - 1) {
            printf(" -> ");
        }
    }
    printf("\n");
}

static void task_add(struct task *task)
{
    list_add_tail(&task->list, &tasklist);
}

static void task_switch()
{
    if (!list_empty(&tasklist)) {
        struct task *t = list_first_entry(&tasklist, struct task, list);
        printf("taskswitch del:%s\n", t->task_name);
        list_del(&t->list);
        cur_task = t;
        longjmp(t->env, 1);
    }
}

void schedule(void)
{
    static int i;
    // int j=0;
    setjmp(sched);
    while (ntasks-- > 0) {
        // printf("schetime %d ntasks:%d\n",j++,ntasks);
        i = (i + 1) % 3;
        printf("schedule i:%d\n", i);
        struct arg arg = args[i];
        tasks[i](&arg);
        printf("Never reached\n");
    }

    task_switch();
}

void task_ai1(void *arg)
{
    struct task *task = malloc(sizeof(struct task));

    strncpy(task->task_name, ((struct arg *) arg)->task_name,
            sizeof(task->task_name) - 1);
    task->task_name[sizeof(task->task_name) - 1] = '\0';

    INIT_LIST_HEAD(&task->list);

    // printf("taskname: %s\n", task->task_name);

    if (setjmp(task->env) == 0) {
        task_add(task);
        longjmp(sched, 1);
    }

    task = cur_task;
    char ai = 'X';
    while (1) {
        if (setjmp(task->env) == 0) {
            if (roundend) {
                break;
            }
            char win = check_win(table);
            if (win == 'D') {
                draw_board(table);
                printf("It is a draw!\n");
                break;
            } else if (win != ' ') {
                draw_board(table);
                printf("%c won!\n", win);
                break;
            }
            int move = mcts(table, ai);
            if (move != -1) {
                table[move] = ai;
                record_move(move);
            }
            draw_board(table);
            task_add(task);
            task_switch();
        }

        task = cur_task;
        // printf("%s: resume\n", task->task_name);
    }
    roundend = true;
    printf("%s: complete\n", task->task_name);
    longjmp(sched, 1);
}

void task_ai2(void *arg)
{
    struct task *task = malloc(sizeof(struct task));

    strncpy(task->task_name, ((struct arg *) arg)->task_name,
            sizeof(task->task_name) - 1);
    task->task_name[sizeof(task->task_name) - 1] = '\0';
    INIT_LIST_HEAD(&task->list);

    // printf("taskname: %s\n", task->task_name);

    if (setjmp(task->env) == 0) {
        task_add(task);
        longjmp(sched, 1);
    }

    task = cur_task;
    char ai = 'O';
    while (1) {
        if (setjmp(task->env) == 0) {
            if (roundend) {
                break;
            }
            char win = check_win(table);
            if (win == 'D') {
                draw_board(table);
                printf("It is a draw!\n");
                break;
            } else if (win != ' ') {
                draw_board(table);
                printf("%c won!\n", win);
                break;
            }
            int move = negamax_predict(table, ai).move;
            if (move != -1) {
                table[move] = ai;
                record_move(move);
            }
            draw_board(table);
            task_add(task);
            task_switch();
        }

        task = cur_task;
        // printf("%s: resume\n", task->task_name);
    }
    roundend = true;
    printf("%s: complete\n", task->task_name);
    longjmp(sched, 1);
}


void task_keyboardevents(void *arg)
{
    struct task *task = malloc(sizeof(struct task));

    strncpy(task->task_name, ((struct arg *) arg)->task_name,
            sizeof(task->task_name) - 1);
    task->task_name[sizeof(task->task_name) - 1] = '\0';
    INIT_LIST_HEAD(&task->list);

    // printf("taskname %s\n", task->task_name);

    if (setjmp(task->env) == 0) {
        task_add(task);
        longjmp(sched, 1);
    }

    task = cur_task;



    printf("%s: complete\n", task->task_name);
    longjmp(sched, 1);
}

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
void coro_ttt(int times)
{
    rounds = times;
    srand(time(NULL));
    memset(table, ' ', N_GRIDS);
    negamax_init();
    void (*registered_task[])(void *) = {task_ai1, task_ai2,
                                         task_keyboardevents};
    struct arg arg0 = {.task_name = "task_ai1"};
    struct arg arg1 = {.task_name = "task_ai2"};
    struct arg arg2 = {.task_name = "task_keyboardevents"};
    struct arg registered_arg[] = {arg0, arg1, arg2};
    tasks = registered_task;
    args = registered_arg;
    ntasks = ARRAY_SIZE(registered_task);

    schedule();
    print_moves();
    move_count = 0;
    roundend = false;
}