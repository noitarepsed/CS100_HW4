#include "snake.h"
#include <stdlib.h>

/* Two directions are opposite when their enum values differ by half the count. */
enum { OPPOSITE_TURN_OFFSET = DIRECTION_COUNT / 2 };

static bool isDirectionValid(Direction direction) {
    return (int)direction >= 0 && (int)direction < DIRECTION_COUNT;
}

static bool isOpposite(Direction first, Direction second) {
    return ((int)first + OPPOSITE_TURN_OFFSET) % DIRECTION_COUNT == (int)second;
}

/* A direct reversal would fold the snake onto itself, so it is ignored unless the
 * snake has one node; an invalid value also keeps the current direction. */
static Direction effectiveDirection(const Snake *snake, Direction requested) {
    if (!isDirectionValid(requested)) {
        return snake->m_direction;
    }
    if (snake->m_length > 1 && isOpposite(requested, snake->m_direction)) {
        return snake->m_direction;
    }
    return requested;
}

static Coord cellAhead(Coord from, Direction direction) {
    Coord pos = from;
    switch (direction) {
    case DIR_UP:
        --pos.m_row;
        break;
    case DIR_DOWN:
        ++pos.m_row;
        break;
    case DIR_LEFT:
        --pos.m_col;
        break;
    case DIR_RIGHT:
        ++pos.m_col;
        break;
    default:
        break;
    }
    return pos;
}

/* Coordinates start at 0 and the outer BOARD_WALL_THICKNESS ring is wall. */
static bool cellIsWall(const Snake *snake, Coord pos) {
    return pos.m_row < BOARD_WALL_THICKNESS || pos.m_row >= snake->m_rows - BOARD_WALL_THICKNESS ||
           pos.m_col < BOARD_WALL_THICKNESS || pos.m_col >= snake->m_cols - BOARD_WALL_THICKNESS;
}

/* Walk the ring from the head for exactly m_length links, so a NULL link is never
 * followed. The tail is the second node visited, so it can only be skipped cell by
 * cell; the walk must continue through the rest of the body afterwards. */
static bool ringContains(const Snake *snake, Coord pos, bool skipTail) {
    if (snake->m_head == NULL || snake->m_length == 0) {
        return false;
    }
    const SnakeNode *tail = snake->m_head->m_next;
    const SnakeNode *node = snake->m_head;
    for (size_t visited = 0; visited < snake->m_length; ++visited) {
        if (!skipTail || node != tail) {
            if (node->m_pos.m_row == pos.m_row && node->m_pos.m_col == pos.m_col) {
                return true;
            }
        }
        node = node->m_next;
    }
    return false;
}

/* The head sits at max(cols / 2, length) so the body always stays inside the wall. */
static int initialHeadColumn(int cols, size_t length) {
    const int halfCols = cols / 2;
    return halfCols >= (int)length ? halfCols : (int)length;
}

/* Free a half-built ring: links run from newest back to the head, and the head
 * still points at itself, so the walk stops there instead of going round. */
static void releaseHalfBuiltRing(SnakeNode *head, SnakeNode *newest) {
    SnakeNode *node = newest;
    while (node != head) {
        SnakeNode *nextNode = node->m_next;
        free(node);
        node = nextNode;
    }
    free(head);
}

bool snakeInit(Snake *snake, int rows, int cols, size_t length) {
    const int bodyLimit = cols - 2 * BOARD_WALL_THICKNESS;
    if (rows < MIN_BOARD_SIZE || cols < MIN_BOARD_SIZE || bodyLimit <= 0 || length == 0 ||
        length > (size_t)bodyLimit) {
        *snake = (Snake){0};
        return false;
    }
    const int row = rows / 2;
    const int headCol = initialHeadColumn(cols, length);
    SnakeNode *head = malloc(sizeof(*head));
    if (head == NULL) {
        *snake = (Snake){0};
        return false;
    }
    head->m_pos = (Coord){row, headCol};
    head->m_next = head;
    /* Nodes are chained from the head outwards: each new node points back towards
     * the head, and the one built last becomes the tail. */
    SnakeNode *newest = head;
    for (size_t offset = 1; offset < length; ++offset) {
        SnakeNode *node = malloc(sizeof(*node));
        if (node == NULL) {
            releaseHalfBuiltRing(head, newest);
            *snake = (Snake){0};
            return false;
        }
        node->m_pos = (Coord){row, headCol - (int)offset};
        node->m_next = newest;
        newest = node;
    }
    head->m_next = newest;
    *snake = (Snake){.m_head = head,
                     .m_length = length,
                     .m_direction = DIR_RIGHT,
                     .m_rows = rows,
                     .m_cols = cols};
    return true;
}

Coord snakeNextPosition(const Snake *snake, Direction requested) {
    return cellAhead(snake->m_head->m_pos, effectiveDirection(snake, requested));
}

bool snakeContains(const Snake *snake, Coord pos) { return ringContains(snake, pos, false); }

StepResult snakeStep(Snake *snake, Direction requested, bool grow) {
    const Direction direction = effectiveDirection(snake, requested);
    const Coord next = cellAhead(snake->m_head->m_pos, direction);
    if (cellIsWall(snake, next)) {
        return STEP_WALL;
    }
    /* Growth keeps every old cell occupied, so the tail cell blocks it; a normal
     * move vacates the tail cell and may therefore enter it. */
    if (ringContains(snake, next, !grow)) {
        return STEP_SELF;
    }
    if (grow) {
        SnakeNode *node = malloc(sizeof(*node));
        if (node == NULL) {
            return STEP_NO_MEMORY;
        }
        /* The new head takes the slot between the old head and the tail. */
        node->m_pos = next;
        node->m_next = snake->m_head->m_next;
        snake->m_head->m_next = node;
        snake->m_head = node;
        ++snake->m_length;
    } else {
        /* Reuse the tail node as the new head; no coordinate moves across the body. */
        SnakeNode *tail = snake->m_head->m_next;
        tail->m_pos = next;
        snake->m_head = tail;
    }
    snake->m_direction = direction;
    return STEP_OK;
}

void snakeDestroy(Snake *snake) {
    SnakeNode *node = snake->m_head;
    for (size_t freed = 0; freed < snake->m_length && node != NULL; ++freed) {
        SnakeNode *nextNode = node->m_next;
        free(node);
        node = nextNode;
    }
    *snake = (Snake){0};
}

unsigned int nextRandom(unsigned int seed) {
    static unsigned int state = RANDOM_INITIAL_SEED;
    if (seed > 0) {
        state = seed % RANDOM_MODULUS;
    }
    state = (RANDOM_MULTIPLIER * state + RANDOM_INCREMENT) % RANDOM_MODULUS;
    return state;
}
