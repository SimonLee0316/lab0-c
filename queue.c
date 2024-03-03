#include <random.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

/* Notice: sometimes, Cppcheck would find the potential NULL pointer bugs,
 * but some of them cannot occur. You can suppress them by adding the
 * following line.
 *   cppcheck-suppress nullPointer
 */


/* Create an empty queue */
struct list_head *q_new()
{
    struct list_head *head = malloc(sizeof(struct list_head));
    if (!head)
        return NULL;

    INIT_LIST_HEAD(head);
    return head;
}

/* Free all storage used by queue */
void q_free(struct list_head *head)
{
    if (!head)
        return;

    struct list_head *current = head->next;

    while (head != current) {
        current = current->next;
        q_release_element(container_of(current->prev, element_t, list));
    }

    free(head);
}

/* Insert an element at head of queue */
bool q_insert_head(struct list_head *head, char *s)
{
    if (!head)
        return false;

    element_t *new = malloc(sizeof(element_t));
    if (!new)
        return false;

    int len = strlen(s);
    new->value = malloc(sizeof(char) * len + 1);
    if (!new->value) {
        q_release_element(new);
        return false;
    }
    strncpy(new->value, s, len + 1);
    list_add(&new->list, head);

    return true;
}

/* Insert an element at tail of queue */
bool q_insert_tail(struct list_head *head, char *s)
{
    if (!head)
        return false;

    element_t *new = malloc(sizeof(element_t));
    if (!new)
        return false;

    int len = strlen(s);
    new->value = malloc(sizeof(char) * len + 1);
    if (!new->value) {
        q_release_element(new);
        return false;
    }
    strncpy(new->value, s, len + 1);
    list_add_tail(&new->list, head);

    return true;
}

/* Remove an element from head of queue */
element_t *q_remove_head(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || q_size(head) == 0)
        return NULL;
    element_t *felement = list_first_entry(head, element_t, list);
    list_del_init(head->next);

    if (sp) {
        strncpy(sp, felement->value, bufsize - 1);
        *(sp + bufsize - 1) = '\0';
    }

    return felement;
}

/* Remove an element from tail of queue */
element_t *q_remove_tail(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || q_size(head) == 0)
        return NULL;
    element_t *Lastelement = list_last_entry(head, element_t, list);
    list_del_init(head->prev);

    if (!Lastelement)
        return NULL;

    if (sp) {
        strncpy(sp, Lastelement->value, bufsize - 1);
        *(sp + bufsize - 1) = '\0';
    }
    return Lastelement;
}

/* Return number of elements in queue */
int q_size(struct list_head *head)
{
    if (!head || list_empty(head))
        return 0;
    int len = 0;

    struct list_head *li;

    list_for_each (li, head)
        len++;
    return len;
}

/* Delete the middle node in queue */
bool q_delete_mid(struct list_head *head)
{
    // https://leetcode.com/problems/delete-the-middle-node-of-a-linked-list/

    if (!head || list_empty(head))
        return false;

    struct list_head *slow = head;

    for (struct list_head *fast = head->next;
         head != fast && head != fast->next; fast = fast->next->next)
        slow = slow->next;

    struct list_head *mid = slow->next;

    list_del_init(mid);
    q_release_element(container_of(mid, element_t, list));

    return true;
}

/* Delete all nodes that have duplicate string */
bool q_delete_dup(struct list_head *head)
{
    // https://leetcode.com/problems/remove-duplicates-from-sorted-list-ii/

    if (!head || list_empty(head))
        return false;

    element_t *current;
    element_t *safe;
    bool flag = false;

    list_for_each_entry_safe (current, safe, head, list) {
        if ((&safe->list != head) &&
            (strcmp(current->value, safe->value) == 0)) {
            list_del_init(&current->list);
            q_release_element(current);
            flag = true;
        } else if (flag) {
            list_del_init(&current->list);
            q_release_element(current);
            flag = false;
        }
    }

    return true;
}

/* Swap every two adjacent nodes */
void q_swap(struct list_head *head)
{
    // https://leetcode.com/problems/swap-nodes-in-pairs/

    if (!head || list_empty(head))
        return;
    element_t *current, *next;
    char *tmp;
    int count = 0;

    list_for_each_entry_safe (current, next, head, list) {
        if (&next->list != head && count % 2 == 0) {
            tmp = current->value;
            current->value = next->value;
            next->value = tmp;
        }
        count++;
    }
}

/* Reverse elements in queue */
void q_reverse(struct list_head *head)
{
    if (!head || list_empty(head))
        return;
    struct list_head *tmp;
    struct list_head *i;
    struct list_head *normal = head->next;
    // struct list_head *headtmp = head;

    for (i = head;; i = normal, normal = normal->next) {
        tmp = i->prev;
        i->prev = i->next;
        i->next = tmp;
        if (i->prev == head)
            break;
    }
}

/* Reverse the nodes of the list k at a time */
void q_reverseK(struct list_head *head, int k)
{
    // https://leetcode.com/problems/reverse-nodes-in-k-group/
    if (!head || list_empty(head) || k <= 1)
        return;
    int len = q_size(head);
    for (struct list_head *i = head->next; i->next != head && i != head;) {
        if (len >= k) {
            struct list_head *tmp = i->next;
            struct list_head *safe = tmp->next;
            struct list_head *iprev = i->prev;
            for (int j = 0; j < k - 1; j++) {
                list_move(tmp, iprev);
                tmp = safe;
                safe = safe->next;
            }
            i = safe->prev;
            len = len - k;
        } else {
            i = i->next;
        }
    }
}

struct list_head *merge(struct list_head *left, struct list_head *right)
{
    struct list_head *dummy = left;
    struct list_head *tail = dummy;
    bool count = false;
    while (left && right) {
        if (strcmp(list_entry(left, element_t, list)->value,
                   list_entry(right, element_t, list)->value) > 0) {
            struct list_head *next_right = right->next;
            if (!count) {
                count = true;
                dummy = right;
                tail = right;
                right->next = left;
                right = next_right;
                continue;
            }

            right->next = left;
            tail->next = right;

            tail = right;
            right = next_right;
        } else if (strcmp(list_entry(left, element_t, list)->value,
                          list_entry(right, element_t, list)->value) <= 0) {
            if (!count) {
                count = true;
                dummy = left;
                tail = left;
                left = left->next;
                continue;
            }
            tail->next = left;
            tail = left;
            left = left->next;
        }
    }

    if (right)
        tail->next = right;
    else
        tail->next = left;

    return dummy;
}


struct list_head *merge_sort(struct list_head *head)
{
    if (!head || !head->next)
        return head;

    struct list_head *slow = head;

    for (struct list_head *fast = head->next; fast && fast->next;
         fast = fast->next->next) {
        slow = slow->next;
    }
    struct list_head *left;
    struct list_head *right = slow->next;

    slow->next = NULL;

    left = merge_sort(head);
    right = merge_sort(right);

    return merge(left, right);
}

/* Sort elements of queue in ascending/descending order */
void q_sort(struct list_head *head, bool descend)
{
    if (!head || list_empty(head) || q_size(head) < 2)
        return;

    head->prev->next = NULL;
    head->next = merge_sort(head->next);


    struct list_head *current = head, *after = head->next;

    while (after) {
        after->prev = current;
        current = after;
        after = after->next;
    }
    current->next = head;
    head->prev = current;
    if (descend)
        q_reverse(head);
}

/* Remove every node which has a node with a strictly less value anywhere to
 * the right side of it */
int q_ascend(struct list_head *head)
{
    // https://leetcode.com/problems/remove-nodes-from-linked-list/
    if (!head || list_empty(head))
        return 0;

    struct list_head *min = head->prev;

    for (struct list_head *li = head->prev->prev; li != head;) {
        if (strcmp(list_entry(min, element_t, list)->value,
                   list_entry(li, element_t, list)->value) <= 0) {
            element_t *delelement = list_entry(li, element_t, list);
            li = li->prev;
            list_del_init(li->next);
            q_release_element(delelement);

        } else {
            min = li;
            li = li->prev;
        }
    }
    return 0;
}

/* Remove every node which has a node with a strictly greater value anywhere to
 * the right side of it */
int q_descend(struct list_head *head)
{
    // https://leetcode.com/problems/remove-nodes-from-linked-list/
    if (!head || list_empty(head))
        return 0;

    struct list_head *max = head->prev;

    for (struct list_head *li = head->prev->prev; li != head;) {
        if (strcmp(list_entry(max, element_t, list)->value,
                   list_entry(li, element_t, list)->value) > 0) {
            element_t *delelement = list_entry(li, element_t, list);
            li = li->prev;
            list_del_init(li->next);
            q_release_element(delelement);

        } else {
            max = li;
            li = li->prev;
        }
    }
    return 0;
}

/* Merge all the queues into one sorted queue, which is in ascending/descending
 * order */
int q_merge(struct list_head *head, bool descend)
{
    // https://leetcode.com/problems/merge-k-sorted-lists/
    if (list_empty(head))
        return 0;
    if (list_is_singular(head))
        return list_entry(head->next, queue_contex_t, chain)->size;

    queue_contex_t *start = list_entry(head->next, queue_contex_t, chain);
    queue_contex_t *li = NULL;

    struct list_head *tmp = start->q->next;
    start->q->prev->next = NULL;
    list_for_each_entry (li, head, chain) {
        if (start == li)
            continue;
        li->q->prev->next = NULL;
        tmp = merge(tmp, li->q->next);
        INIT_LIST_HEAD(li->q);
    }

    start->q->next = tmp;

    struct list_head *current = start->q, *after = start->q->next;

    while (after) {
        after->prev = current;
        current = after;
        after = after->next;
    }
    current->next = start->q;
    start->q->prev = current;

    return q_size(start->q);
}

void swap(struct list_head *a, struct list_head *b)
{
    char *tmp;
    tmp = list_entry(a, element_t, list)->value;

    list_entry(a, element_t, list)->value =
        list_entry(b, element_t, list)->value;
    list_entry(b, element_t, list)->value = tmp;
}

/* Generate random number from 1 to k inclusive */
static inline unsigned int get_random_range_1_to_k(unsigned int k)
{
    unsigned int rand_val = 0;
    randombytes((uint8_t *) &rand_val, sizeof(rand_val));
    return 1 + (rand_val % k);
}

bool q_shuffle(struct list_head *head)
{
    struct list_head *tail = head->prev;
    struct list_head *current = head->next;

    for (int i = q_size(head); i > 0; i--) {
        int rand_number = get_random_range_1_to_k(i);
        current = head->next;
        for (int j = 1; j < rand_number; j++) {
            current = current->next;
        }
        swap(current, tail);
        tail = tail->prev;
    }

    return true;
}
