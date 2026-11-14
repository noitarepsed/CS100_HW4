/* Boundary tests for the P1 Snake model. This driver has its own main, so it lives
 * in p1/extras/ where the grader never links it with its test runner.
 * Build from p1/: cc -std=c17 -Wall -Wextra -Wpedantic -fsanitize=address,undefined \
 *   extras/test_snake.c snake.c -o build/test-snake && ./build/test-snake */
#include "snake.h"
#include <stdio.h>
#include <stdlib.h>

static int g_failures = 0;

static void check(bool condition, const char *message) {
    if (!condition) {
        printf("FAIL: %s\n", message);
        ++g_failures;
    }
}

/* Every ring must return to the head after exactly m_length links. */
static bool ringIsValid(const Snake *snake) {
    if (snake->m_length == 0) {
        return snake->m_head == NULL;
    }
    if (snake->m_head == NULL) {
        return false;
    }
    const SnakeNode *node = snake->m_head;
    for (size_t step = 0; step < snake->m_length; ++step) {
        if (node == NULL) {
            return false;
        }
        if (step > 0 && node == snake->m_head) {
            return false;
        }
        node = node->m_next;
    }
    return node == snake->m_head;
}

static bool hasCell(const Snake *snake, int row, int col) {
    return snakeContains(snake, (Coord){row, col});
}

/* A length-n ring must close after exactly n links from any node, not only the head. */
static bool ringClosesFromEveryNode(const Snake *snake) {
    if (snake->m_length == 0) {
        return snake->m_head == NULL;
    }
    const SnakeNode *start = snake->m_head;
    for (size_t nodeIndex = 0; nodeIndex < snake->m_length; ++nodeIndex) {
        const SnakeNode *node = start;
        for (size_t link = 0; link < snake->m_length; ++link) {
            if (node == NULL || (link > 0 && node == start)) {
                return false;
            }
            node = node->m_next;
        }
        if (node != start) {
            return false;
        }
        start = start->m_next;
    }
    return true;
}

/* Hand-build a ring in traversal order: ring[0] is the head, ring[1] the tail, and
 * the last entry is the node adjacent to the head. */
static bool buildRing(Snake *snake, int rows, int cols, Direction direction, const Coord *ring,
                      size_t length) {
    SnakeNode **nodes = malloc(length * sizeof(*nodes));
    if (nodes == NULL) {
        return false;
    }
    size_t built = 0;
    for (; built < length; ++built) {
        nodes[built] = malloc(sizeof(*nodes[built]));
        if (nodes[built] == NULL) {
            break;
        }
        nodes[built]->m_pos = ring[built];
        nodes[built]->m_next = NULL;
    }
    if (built < length) {
        for (size_t i = 0; i < built; ++i) {
            free(nodes[i]);
        }
        free(nodes);
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        nodes[i]->m_next = nodes[(i + 1) % length];
    }
    *snake = (Snake){.m_head = nodes[0],
                     .m_length = length,
                     .m_direction = direction,
                     .m_rows = rows,
                     .m_cols = cols};
    free(nodes);
    return true;
}

static void testRandomDefault(void) {
    check(nextRandom(0) == 11921, "default state, first call");
    check(nextRandom(0) == 22458, "default state, second call");
}

static void testRandomSeeded(void) {
    check(nextRandom(1) == 1130, "seed 1");
    check(nextRandom(0) == 25887, "seed 1 then zero");
    check(nextRandom(0) == 4656, "seed 1 then two zeros");
    check(nextRandom(1) == 1130, "reseed 1 repeats the value");
    check(nextRandom(32768) == 1021, "seed equal to the modulus normalizes to zero");
}

static void testRandomPeriod(void) {
    unsigned int first = nextRandom(1);
    size_t steps = 1;
    unsigned int value = first;
    do {
        value = nextRandom(0);
        ++steps;
    } while (value != first && steps <= RANDOM_MODULUS + 1);
    /* The state after seeding is s0; the first value is f(s0), so it reappears only
     * after 32768 further steps: 32769 outputs before the sequence repeats. */
    check(steps == RANDOM_MODULUS + 1, "generator has the full period 32768");
    check(value < RANDOM_MODULUS, "values stay inside the modulus");
}

static void testInitGeometry(void) {
    Snake snake = {0};
    check(snakeInit(&snake, 12, 24, 3), "init accepts a 12x24 board with length 3");
    check(snake.m_rows == 12 && snake.m_cols == 24, "board size is stored");
    check(snake.m_direction == DIR_RIGHT, "init faces right");
    check(snake.m_length == 3, "length is stored");
    check(snake.m_head != NULL && snake.m_head->m_pos.m_row == 6 && snake.m_head->m_pos.m_col == 12,
          "head sits at row rows/2, column max(cols/2, length)");
    check(hasCell(&snake, 6, 12) && hasCell(&snake, 6, 11) && hasCell(&snake, 6, 10),
          "body extends left one column per node");
    check(!hasCell(&snake, 6, 13) && !hasCell(&snake, 5, 12), "no cells outside the body");
    check(snake.m_head->m_next->m_pos.m_col == 10, "head->next is the tail");
    check(ringIsValid(&snake), "init produces a valid ring");
    snakeDestroy(&snake);
    check(snake.m_head == NULL && snake.m_length == 0 && snake.m_direction == 0 &&
              snake.m_rows == 0 && snake.m_cols == 0,
          "destroy zeroes every field");
}

static void testInitLongest(void) {
    Snake snake = {0};
    check(snakeInit(&snake, 7, 7, 5), "init accepts the longest legal body");
    check(snake.m_head->m_pos.m_col == 5, "head column grows to the body length");
    check(hasCell(&snake, 3, 1), "the tail stops just inside the wall");
    check(ringIsValid(&snake), "longest body keeps a valid ring");
    snakeDestroy(&snake);
}

static void testInitRejects(void) {
    Snake snake = {0};
    check(!snakeInit(&snake, 5, 7, 1), "rejects rows below MIN_BOARD_SIZE");
    check(snake.m_head == NULL && snake.m_length == 0, "rejected init leaves an empty snake");
    check(!snakeInit(&snake, 7, 5, 1), "rejects cols below MIN_BOARD_SIZE");
    check(!snakeInit(&snake, 7, 7, 0), "rejects length zero");
    check(!snakeInit(&snake, 7, 7, 6), "rejects a body longer than cols - 2");
    check(snake.m_head == NULL && snake.m_length == 0, "every rejection leaves an empty snake");
}

static void testInitMinimumBoard(void) {
    Snake snake = {0};
    check(snakeInit(&snake, 6, 6, 4), "init on the smallest board with the longest body");
    check(snake.m_head->m_pos.m_row == 3 && snake.m_head->m_pos.m_col == 4,
          "the head column is max(cols/2, length) on a 6x6 board");
    check(hasCell(&snake, 3, 1), "the tail sits next to the left wall");
    check(ringClosesFromEveryNode(&snake), "the smallest ring closes from every node");
    snakeDestroy(&snake);

    check(snakeInit(&snake, 6, 6, 1), "init a single node on the smallest board");
    check(snake.m_head->m_pos.m_row == 3 && snake.m_head->m_pos.m_col == 3,
          "a single node sits at cols/2 when it is the wider term");
    check(snake.m_head->m_next == snake.m_head, "a single node points at itself");
    check(ringClosesFromEveryNode(&snake), "the one-node ring closes from its only node");
    snakeDestroy(&snake);
}

static void testNextPosition(void) {
    Snake snake = {0};
    check(snakeInit(&snake, 12, 24, 3), "init for next-position checks");
    check(snakeNextPosition(&snake, DIR_UP).m_row == 5, "up decreases the row");
    check(snakeNextPosition(&snake, DIR_DOWN).m_row == 7, "down increases the row");
    check(snakeNextPosition(&snake, DIR_RIGHT).m_col == 13, "right increases the column");
    check(snakeNextPosition(&snake, DIR_LEFT).m_col == 13, "a reverse turn is ignored");
    check(snakeNextPosition(&snake, (Direction)DIRECTION_COUNT).m_col == 13,
          "an invalid direction keeps the current heading");
    check(snakeNextPosition(&snake, (Direction)-1).m_col == 13,
          "a negative direction keeps the current heading");
    check(snake.m_direction == DIR_RIGHT && snake.m_head->m_pos.m_col == 12,
          "next-position never mutates the snake");
    snakeDestroy(&snake);

    Snake single = {0};
    check(snakeInit(&single, 12, 24, 1), "init a one-node snake");
    check(snakeNextPosition(&single, DIR_LEFT).m_col == 11, "a one-node snake may reverse");
    snakeDestroy(&single);
}

static void testNextPositionAllDirections(void) {
    Snake snake = {0};
    check(snakeInit(&snake, 9, 9, 1), "init a single node for all four directions");
    check(snakeNextPosition(&snake, DIR_UP).m_row == 3 &&
              snakeNextPosition(&snake, DIR_UP).m_col == 4,
          "up from a single node");
    check(snakeNextPosition(&snake, DIR_DOWN).m_row == 5, "down from a single node");
    check(snakeNextPosition(&snake, DIR_LEFT).m_col == 3, "left from a single node");
    check(snakeNextPosition(&snake, DIR_RIGHT).m_col == 5, "right from a single node");
    snakeDestroy(&snake);
}

static void testContains(void) {
    Snake snake = {0};
    Snake empty = {0};
    check(!snakeContains(&empty, (Coord){6, 12}), "an empty snake contains nothing");
    check(snakeInit(&snake, 12, 24, 3), "init for contains checks");
    check(hasCell(&snake, 6, 12) && hasCell(&snake, 6, 10), "occupied cells are found");
    check(!hasCell(&snake, 7, 11), "absent cells are rejected");
    check(!hasCell(&snake, 6, 13), "a row-only and column-only match is rejected");
    check(!hasCell(&snake, 0, 12), "the wall row is not part of the body");
    snakeDestroy(&snake);
}

static void testStepNormal(void) {
    Snake snake = {0};
    check(snakeInit(&snake, 12, 24, 3), "init for normal-move checks");
    SnakeNode *oldTail = snake.m_head->m_next;
    check(snakeStep(&snake, DIR_RIGHT, false) == STEP_OK, "a normal move succeeds");
    check(snake.m_head == oldTail, "the old tail node becomes the new head");
    check(snake.m_length == 3, "a normal move keeps the length");
    check(hasCell(&snake, 6, 13) && hasCell(&snake, 6, 12) && !hasCell(&snake, 6, 10),
          "the body shifted by one cell");
    check(snake.m_direction == DIR_RIGHT, "the effective direction is stored");
    check(ringIsValid(&snake), "a normal move keeps a valid ring");
    snakeDestroy(&snake);
}

static void testStepWall(void) {
    Snake snake = {0};
    check(snakeInit(&snake, 7, 9, 1), "init near the right wall");
    check(snakeStep(&snake, DIR_RIGHT, false) == STEP_OK, "move inside the board");
    check(snakeStep(&snake, DIR_RIGHT, false) == STEP_OK, "keep moving towards the wall");
    check(snakeStep(&snake, DIR_RIGHT, false) == STEP_OK, "move up to the last free cell");
    check(snake.m_head->m_pos.m_col == 7, "the head reached cols - 2");
    check(snakeStep(&snake, DIR_RIGHT, false) == STEP_WALL, "the wall column ends the move");
    check(snake.m_head->m_pos.m_col == 7 && snake.m_length == 1,
          "a wall failure leaves the snake unchanged");
    check(snakeStep(&snake, DIR_UP, false) == STEP_OK, "a turn away from the wall succeeds");
    check(snakeStep(&snake, DIR_UP, false) == STEP_OK, "climb to the first free row");
    check(snake.m_head->m_pos.m_row == 1, "the head reached the wall row plus one");
    check(snakeStep(&snake, DIR_UP, false) == STEP_WALL, "the wall row ends the move");
    snakeDestroy(&snake);
}

static void testStepSelf(void) {
    Snake snake = {0};
    /* Ring order: head (4,4) -> tail (4,5) -> (3,5) -> (3,4) -> head. The cell ahead
     * of a right-facing head is the tail cell. */
    const Coord ring[] = {{4, 4}, {4, 5}, {3, 5}, {3, 4}};
    check(buildRing(&snake, 9, 9, DIR_RIGHT, ring, 4), "build a looped snake");
    SnakeNode *headBefore = snake.m_head;
    check(snakeStep(&snake, DIR_RIGHT, true) == STEP_SELF, "growth may not enter the tail cell");
    check(snake.m_head == headBefore && snake.m_length == 4 && snake.m_head->m_pos.m_row == 4 &&
              snake.m_head->m_pos.m_col == 4,
          "a self collision leaves the snake unchanged");
    check(ringIsValid(&snake), "a self collision keeps a valid ring");
    snakeDestroy(&snake);
}

static void testStepTailCell(void) {
    Snake snake = {0};
    const Coord ring[] = {{4, 4}, {4, 5}, {3, 5}, {3, 4}};
    check(buildRing(&snake, 9, 9, DIR_RIGHT, ring, 4), "build a looped snake");
    check(snakeStep(&snake, DIR_RIGHT, false) == STEP_OK, "a normal move may enter the tail cell");
    check(snake.m_head->m_pos.m_col == 5 && snake.m_length == 4,
          "the head took the vacated tail cell");
    check(hasCell(&snake, 4, 4) && hasCell(&snake, 3, 5) && hasCell(&snake, 4, 5),
          "the old body cells are still occupied");
    check(ringIsValid(&snake), "moving into the tail cell keeps a valid ring");
    snakeDestroy(&snake);
}

/* The body must be checked cell by cell: only the tail cell is exempt, everything
 * else that is occupied has to stop a normal move and a growing move alike. */
static void testStepSelfCoverage(void) {
    Snake snake = {0};
    /* Head (4,4) faces up into (3,4), the last ring node right next to the head. */
    const Coord adjacentRing[] = {{4, 4}, {4, 5}, {3, 5}, {3, 4}};
    check(buildRing(&snake, 9, 9, DIR_UP, adjacentRing, 4), "build a snake facing its own body");
    check(snakeStep(&snake, DIR_UP, false) == STEP_SELF, "a normal move may not enter a body cell");
    check(snake.m_head->m_pos.m_row == 4 && snake.m_length == 4,
          "the rejected move kept the snake");
    snakeDestroy(&snake);

    /* Here the occupied target cell sits in the middle of the ring, not at its end. */
    const Coord middleRing[] = {{4, 4}, {4, 5}, {5, 5}, {5, 4}, {3, 4}, {3, 5}};
    check(buildRing(&snake, 9, 9, DIR_UP, middleRing, 6), "build a snake with a middle cell ahead");
    check(snakeStep(&snake, DIR_UP, true) == STEP_SELF, "growth may not enter a middle body cell");
    check(snakeStep(&snake, DIR_UP, false) == STEP_SELF,
          "a normal move may not enter a middle cell");
    check(snake.m_head->m_pos.m_row == 4 && snake.m_length == 6, "rejected moves keep the snake");
    check(ringIsValid(&snake), "rejected moves keep a valid ring");
    snakeDestroy(&snake);
}

static void testStepGrow(void) {
    Snake snake = {0};
    check(snakeInit(&snake, 12, 24, 1), "init a one-node snake for growth");
    SnakeNode *oldHead = snake.m_head;
    check(snakeStep(&snake, DIR_RIGHT, true) == STEP_OK, "growth of a one-node snake succeeds");
    check(snake.m_length == 2, "growth adds exactly one node");
    check(snake.m_head->m_pos.m_col == 13, "the new head takes the next cell");
    check(snake.m_head->m_next == oldHead, "the new head points at the old head, the new tail");
    check(oldHead->m_pos.m_col == 12, "growth leaves the old node where it was");
    check(ringIsValid(&snake), "growth keeps a valid ring");

    check(snakeStep(&snake, DIR_DOWN, true) == STEP_OK, "growth along a new direction");
    check(snake.m_length == 3 && hasCell(&snake, 7, 13) && hasCell(&snake, 6, 12),
          "growth keeps every old cell");
    check(ringIsValid(&snake), "growth again keeps a valid ring");

    check(snakeStep(&snake, DIR_UP, false) == STEP_OK, "a reversed request is ignored in step");
    check(snake.m_head->m_pos.m_row == 8 && snake.m_head->m_pos.m_col == 13,
          "the ignored request keeps the downward heading");
    snakeDestroy(&snake);
}

static void testStepGrowAtWall(void) {
    Snake snake = {0};
    const Coord atWall[] = {{1, 7}};
    check(buildRing(&snake, 9, 9, DIR_RIGHT, atWall, 1), "build a node next to the right wall");
    check(snakeStep(&snake, DIR_RIGHT, true) == STEP_WALL, "growth at the wall reports the wall");
    check(snake.m_length == 1 && snake.m_head->m_pos.m_col == 7,
          "growth at the wall changes nothing");
    check(snakeStep(&snake, DIR_RIGHT, false) == STEP_WALL, "a normal move at the wall reports it");
    snakeDestroy(&snake);

    const Coord inside[] = {{1, 6}};
    check(buildRing(&snake, 9, 9, DIR_RIGHT, inside, 1), "build a node one cell inside");
    check(snakeStep(&snake, DIR_RIGHT, true) == STEP_OK, "growth inside the board succeeds");
    check(snake.m_length == 2 && snake.m_head->m_pos.m_col == 7,
          "growth moved the head to the last free column");
    check(ringClosesFromEveryNode(&snake), "the grown ring closes from every node");
    snakeDestroy(&snake);
}

/* A one-node snake may reverse, and an invalid direction never changes the heading. */
static void testStepDirectionEdge(void) {
    Snake snake = {0};
    check(snakeInit(&snake, 9, 9, 1), "init a single node for a reverse step");
    check(snakeStep(&snake, DIR_LEFT, false) == STEP_OK, "a single node may reverse");
    check(snake.m_head->m_pos.m_col == 3 && snake.m_direction == DIR_LEFT,
          "the reverse moves the head left and stores the new heading");
    check(snakeStep(&snake, (Direction)DIRECTION_COUNT, false) == STEP_OK,
          "an invalid direction still moves one cell");
    check(snake.m_head->m_pos.m_col == 2 && snake.m_direction == DIR_LEFT,
          "the invalid direction keeps the previous heading");
    snakeDestroy(&snake);
}

static void testDestroySafety(void) {
    Snake snake = {0};
    snakeDestroy(&snake);
    check(snake.m_head == NULL && snake.m_length == 0, "destroy on an empty snake is safe");
    check(snakeInit(&snake, 7, 9, 4), "init for a repeated destroy");
    snakeDestroy(&snake);
    snakeDestroy(&snake);
    check(snake.m_head == NULL && snake.m_length == 0, "the second destroy is safe");
}

/* An independent reference model of the snake, kept as a head-first list of cells.
 * The simulation replays a fixed move script and compares every result and every
 * occupied cell with this model, so a body cell that the ring walk misses shows up. */
enum { MODEL_CAPACITY = 64, SIMULATION_STEPS = 240, SIM_ROWS = 12, SIM_COLS = 12 };

typedef struct {
    Coord m_cells[MODEL_CAPACITY];
    size_t m_length;
    Direction m_direction;
} Model;

static bool modelContains(const Model *model, Coord pos, bool skipTail) {
    for (size_t i = 0; i < model->m_length; ++i) {
        if (skipTail && i + 1 == model->m_length) {
            continue;
        }
        if (model->m_cells[i].m_row == pos.m_row && model->m_cells[i].m_col == pos.m_col) {
            return true;
        }
    }
    return false;
}

static Direction modelDirection(const Model *model, Direction requested) {
    if ((int)requested < 0 || (int)requested >= DIRECTION_COUNT) {
        return model->m_direction;
    }
    if (model->m_length > 1 &&
        ((int)requested + DIRECTION_COUNT / 2) % DIRECTION_COUNT == (int)model->m_direction) {
        return model->m_direction;
    }
    return requested;
}

static Coord modelNext(const Model *model, Direction direction) {
    Coord pos = model->m_cells[0];
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

static bool modelIsWall(Coord pos) {
    return pos.m_row < BOARD_WALL_THICKNESS || pos.m_row >= SIM_ROWS - BOARD_WALL_THICKNESS ||
           pos.m_col < BOARD_WALL_THICKNESS || pos.m_col >= SIM_COLS - BOARD_WALL_THICKNESS;
}

static void testSimulation(void) {
    static const struct {
        Direction m_dir;
        bool m_grow;
    } script[] = {
        {DIR_RIGHT, false}, {DIR_DOWN, false},  {DIR_LEFT, false}, {DIR_UP, false},
        {DIR_RIGHT, true},  {DIR_DOWN, true},   {DIR_UP, false},   {DIR_LEFT, true},
        {DIR_DOWN, false},  {DIR_RIGHT, false}, {DIR_LEFT, false}, {DIR_UP, true},
    };
    const size_t scriptLength = sizeof(script) / sizeof(script[0]);
    Snake snake = {0};
    Model model = {.m_length = 4, .m_direction = DIR_RIGHT};
    check(snakeInit(&snake, SIM_ROWS, SIM_COLS, model.m_length), "init for the simulation");
    for (size_t i = 0; i < model.m_length; ++i) {
        model.m_cells[i] = (Coord){SIM_ROWS / 2, SIM_COLS / 2 - (int)i};
    }
    for (size_t step = 0; step < SIMULATION_STEPS; ++step) {
        const Direction requested = script[step % scriptLength].m_dir;
        const Direction expectedDir = modelDirection(&model, requested);
        const Coord next = modelNext(&model, expectedDir);
        const bool grow = script[step % scriptLength].m_grow && model.m_length < MODEL_CAPACITY;
        StepResult expected = STEP_OK;
        if (modelIsWall(next)) {
            expected = STEP_WALL;
        } else if (modelContains(&model, next, !grow)) {
            expected = STEP_SELF;
        }
        const StepResult actual = snakeStep(&snake, requested, grow);
        if (actual != expected) {
            printf("FAIL at simulation step %zu: expected %d, got %d\n", step, (int)expected,
                   (int)actual);
            ++g_failures;
        }
        if (expected == STEP_OK) {
            const size_t kept = grow ? model.m_length : model.m_length - 1;
            for (size_t i = kept; i > 0; --i) {
                model.m_cells[i] = model.m_cells[i - 1];
            }
            model.m_cells[0] = next;
            model.m_length = grow ? model.m_length + 1 : model.m_length;
            model.m_direction = expectedDir;
        }
        check(ringClosesFromEveryNode(&snake),
              "the ring closes from every node during the simulation");
        check(snake.m_length == model.m_length, "the simulated length matches the model");
        check(snake.m_direction == model.m_direction, "the simulated direction matches the model");
        check(snake.m_head->m_pos.m_row == model.m_cells[0].m_row &&
                  snake.m_head->m_pos.m_col == model.m_cells[0].m_col,
              "the simulated head matches the model");
        size_t found = 0;
        for (size_t i = 0; i < model.m_length; ++i) {
            found += snakeContains(&snake, model.m_cells[i]) ? 1 : 0;
        }
        check(found == model.m_length, "the snake occupies exactly the model cells");
    }
    snakeDestroy(&snake);
}

int main(void) {
    testRandomDefault();
    testRandomSeeded();
    testRandomPeriod();
    testInitGeometry();
    testInitLongest();
    testInitRejects();
    testInitMinimumBoard();
    testNextPosition();
    testNextPositionAllDirections();
    testContains();
    testStepNormal();
    testStepWall();
    testStepSelf();
    testStepSelfCoverage();
    testStepTailCell();
    testStepGrow();
    testStepGrowAtWall();
    testStepDirectionEdge();
    testDestroySafety();
    testSimulation();
    if (g_failures == 0) {
        printf("All P1 boundary tests passed.\n");
        return 0;
    }
    printf("%d P1 boundary check(s) failed.\n", g_failures);
    return 1;
}
