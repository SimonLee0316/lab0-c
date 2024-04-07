#include "corottt.h"

#include <fcntl.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
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
static int i;
static bool stop = false;

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
        list_del(&t->list);
        cur_task = t;
        longjmp(t->env, 1);
    }
}

void schedule(void)
{
    i = 0;
    setjmp(sched);
    while (ntasks-- > 0) {
        struct arg arg = args[i];
        tasks[i++](&arg);
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


    if (setjmp(task->env) == 0) {
        task_add(task);
        longjmp(sched, 1);
    }

    task = cur_task;

    if (rounds <= 0) {  // if games rounds is end
        longjmp(sched, 1);
    }

    char ai = 'X';
    int move = mcts(table, ai);
    if (move != -1) {
        table[move] = ai;
        record_move(move);
    }

    task_add(task);
    task_switch();
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

    if (rounds <= 0) {
        longjmp(sched, 1);
    }
    char ai = 'O';
    int move = negamax_predict(table, ai).move;
    if (move != -1) {
        table[move] = ai;
        record_move(move);
    }

    task_add(task);
    task_switch();
}

void task_drawboard_checkwin(void *arg)
{
    struct task *task = malloc(sizeof(struct task));

    strncpy(task->task_name, ((struct arg *) arg)->task_name,
            sizeof(task->task_name) - 1);
    task->task_name[sizeof(task->task_name) - 1] = '\0';
    INIT_LIST_HEAD(&task->list);

    if (setjmp(task->env) == 0) {
        task_add(task);
        longjmp(sched, 1);
    }
    task = cur_task;

    if (rounds <= 0) {  // if games rounds is end
        longjmp(sched, 1);
    }

    char win = check_win(table);
    if (win == 'D') {
        printf("It is a draw!\n");
        roundend = true;  // current games is end
        rounds--;
    } else if (win != ' ') {
        printf("%c won!\n", win);
        roundend = true;
        rounds--;
    }
    if (!stop) {  // if press ctr+p stop display table
        draw_board(table);
    }

    if (rounds > 0) {
        task_add(task);
    }
    if (roundend) {  // current games is end,reset the game
        print_moves();
        move_count = 0;
        memset(table, ' ', N_GRIDS);
        roundend = false;
    }

    task_switch();
}

void task_keyboardevents(void *arg)
{
    struct task *task = malloc(sizeof(struct task));

    strncpy(task->task_name, ((struct arg *) arg)->task_name,
            sizeof(task->task_name) - 1);
    task->task_name[sizeof(task->task_name) - 1] = '\0';
    INIT_LIST_HEAD(&task->list);

    if (setjmp(task->env) == 0) {
        task_add(task);
        longjmp(sched, 1);
    }
    task = cur_task;
    if (rounds <= 0) {
        longjmp(sched, 1);
    }

    char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        if (c == 16) {         // ctr+p
            stop = !stop;      // stop or resume
        } else if (c == 17) {  // ctr+q
            rounds = 0;        // set rounds to 0 represent the games is over
        }
    }

    if (rounds > 0) {
        task_add(task);
    }
    if (roundend) {
        move_count = 0;
        memset(table, ' ', N_GRIDS);
        roundend = false;
    }

    task_switch();
}



/* Enable raw input mode */
void enableRawInputMode()
{
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);

    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_iflag &= ~(IXON);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
}

/* disable raw input mode */
void disableRawInputMode()
{
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);

    term.c_lflag |= (ECHO | ICANON);
    term.c_iflag |= (IXON);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &term);

    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) & ~O_NONBLOCK);
}



#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
void coro_ttt(int times)
{
    rounds = times;
    srand(time(NULL));
    memset(table, ' ', N_GRIDS);
    negamax_init();
    void (*registered_task[])(void *) = {task_ai1,
                                         task_keyboardevents,
                                         task_ai2,
                                         task_keyboardevents,
                                         task_drawboard_checkwin,
                                         task_keyboardevents

    };
    struct arg arg0 = {.task_name = "task_ai1"};
    struct arg arg1 = {.task_name = "task_ai2"};
    struct arg arg2 = {.task_name = "task_drawboard_checkwin"};
    struct arg arg3 = {.task_name = "task_keyboardevents"};
    struct arg registered_arg[] = {arg0, arg3, arg1, arg3, arg2, arg3};
    tasks = registered_task;
    args = registered_arg;
    ntasks = ARRAY_SIZE(registered_task);

    enableRawInputMode();
    schedule();
    disableRawInputMode();
}