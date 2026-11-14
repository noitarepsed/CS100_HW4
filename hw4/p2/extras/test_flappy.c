#include "flappy.h"
#include "game.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

enum {
    TEST_STEPS = 12000,
    RANDOM_PROBES = 5000,
    TEST_SCORE_START = 5,
    DEMO_MIN_SCORE = 10,
    /* Mirrors the clamp inside the framework's gap generator. */
    GAP_MARGIN_MIRROR = 56,
    GAP_LOWEST = GAP_MARGIN_MIRROR,
    GAP_HIGHEST = FLOOR_Y - PIPE_GAP_HEIGHT - GAP_MARGIN_MIRROR
};
static const float DISTANCE_TOLERANCE = 1e-4f;
static const float SAFE_GAP_MARGIN = 1.0f;

static int g_failures;

static void check(int condition, const char *what) {
    if (!condition) {
        printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

static bool near(float actual, float expected) {
    return fabsf(actual - expected) <= DISTANCE_TOLERANCE;
}

static void clearPipes(Game *game) {
    for (int i = 0; i < PIPE_COUNT; ++i) {
        game->m_pipes[i] = (Pipe){5000.0f, (float)(FLOOR_Y - PIPE_GAP_HEIGHT) / 2.0f, false};
    }
}

static void testRandomDefaults(void) {
    unsigned int first = nextRandom(0);
    unsigned int second = nextRandom(0);
    check(first == 11921, "the first unseeded call must return 11921");
    check(second == 22458, "the second unseeded call must return 22458");
}

static void testRandomSeeds(void) {
    unsigned int a = nextRandom(1);
    unsigned int b = nextRandom(0);
    unsigned int c = nextRandom(0);
    check(a == 1130 && b == 25887 && c == 4656, "published example 1130/25887/4656");
    check(nextRandom(1) == 1130, "seeding again must restart the sequence");
    check(nextRandom(32768) == 1021, "nextRandom(32768) must return 1021");
    check(nextRandom(RANDOM_MODULUS + 1) == nextRandom(1),
          "large seeds normalize before recurring");
}

static void testRandomRange(void) {
    for (int i = 0; i < RANDOM_PROBES; ++i) {
        if (nextRandom(0) >= RANDOM_MODULUS) {
            check(false, "every result must stay below RANDOM_MODULUS");
            return;
        }
    }

    unsigned int start = nextRandom(1);
    unsigned int value = start;
    for (int i = 0; i < RANDOM_MODULUS; ++i) {
        value = nextRandom(0);
    }
    check(value == start, "the generator must repeat after a full period of 32768");
}

static void testAdvanceMovement(void) {
    Game game = {0};
    float positions[PIPE_COUNT] = {100.0f, 200.0f, 300.0f, 400.0f};
    float gaps[PIPE_COUNT] = {60.0f, 90.0f, 120.0f, 150.0f};
    for (int i = 0; i < PIPE_COUNT; ++i) {
        game.m_pipes[i] = (Pipe){positions[i], gaps[i], false};
    }
    game.m_pipes[2].m_scored = true;
    advancePipes(&game, STEP);
    for (int i = 0; i < PIPE_COUNT; ++i) {
        check(near(game.m_pipes[i].m_x, positions[i] - PIPE_SPEED * STEP), "each pipe moves left");
        check(near(game.m_pipes[i].m_gap_y, gaps[i]), "moving keeps the gap");
    }
    check(game.m_pipes[2].m_scored, "moving keeps the scored flag");
    check(!game.m_pipes[0].m_scored, "moving does not set the scored flag");
}

static void testAdvanceZeroTimeAtTheEdge(void) {
    Game game = {0};
    clearPipes(&game);
    game.m_pipes[0].m_x = -(float)PIPE_WIDTH;
    game.m_pipes[1].m_x = 300.0f;
    game.m_pipes[2].m_x = 400.0f;
    game.m_pipes[3].m_x = 500.0f;
    advancePipes(&game, 0.0f);
    check(near(game.m_pipes[0].m_x, (float)VIEW_W + (float)PIPE_SPACING),
          "a zero step still recycles a pipe whose right edge is exactly zero");
    check(!game.m_pipes[0].m_scored, "recycling must clear the scored flag");
    check(game.m_pipes[0].m_gap_y >= (float)GAP_LOWEST && game.m_pipes[0].m_gap_y <= GAP_HIGHEST,
          "a recycled gap stays inside the playable band");
    check(near(game.m_pipes[1].m_x, 300.0f), "a zero step does not move other pipes");
}

static void testAdvanceRecyclesAfterMoving(void) {
    Game game = {0};
    clearPipes(&game);
    game.m_pipes[0].m_x = -100.0f;
    game.m_pipes[1].m_x = 600.0f;
    game.m_pipes[2].m_x = 700.0f;
    game.m_pipes[3].m_x = 1000.0f;
    advancePipes(&game, STEP);
    const float rightmost = 1000.0f - PIPE_SPEED * STEP;
    check(near(game.m_pipes[0].m_x, rightmost + (float)PIPE_SPACING),
          "the recycled slot goes behind the pipe moved to the rightmost position");
    check(near(game.m_pipes[3].m_x, rightmost), "the rightmost pipe itself moves left");
}

static void testAdvanceSeveralSlots(void) {
    Game game = {0};
    clearPipes(&game);
    game.m_pipes[0].m_x = -(float)PIPE_WIDTH;
    game.m_pipes[1].m_x = -100.0f;
    game.m_pipes[2].m_x = 600.0f;
    game.m_pipes[3].m_x = 700.0f;
    advancePipes(&game, 0.0f);
    check(near(game.m_pipes[0].m_x, (float)VIEW_W + (float)PIPE_SPACING),
          "the first eligible slot lands after the view");
    check(near(game.m_pipes[1].m_x, (float)VIEW_W + 2.0f * (float)PIPE_SPACING),
          "the second eligible slot lands one spacing behind the first");
    check(near(game.m_pipes[2].m_x, 600.0f) && near(game.m_pipes[3].m_x, 700.0f),
          "slots that stay in the view keep their positions");
}

static void testBirdBounds(void) {
    Game game = {0};
    clearPipes(&game);
    game.m_bird_y = 0.0f;
    check(!birdHitsObstacle(&game), "touching the ceiling is safe");
    game.m_bird_y = -0.01f;
    check(birdHitsObstacle(&game), "crossing the ceiling is a collision");

    game.m_bird_y = (float)(FLOOR_Y - BIRD_HEIGHT);
    check(!birdHitsObstacle(&game), "touching the floor is safe");
    game.m_bird_y = (float)(FLOOR_Y - BIRD_HEIGHT) + 0.01f;
    check(birdHitsObstacle(&game), "crossing the floor is a collision");
}

static void testBirdGapIsSafe(void) {
    Game game = {0};
    clearPipes(&game);
    game.m_bird_y = 150.0f;
    game.m_pipes[0].m_x = (float)BIRD_X;
    game.m_pipes[0].m_gap_y = game.m_bird_y - SAFE_GAP_MARGIN;
    check(!birdHitsObstacle(&game), "the bird fits completely inside the gap");
    game.m_pipes[0].m_gap_y = (float)(FLOOR_Y - PIPE_GAP_HEIGHT);
    check(birdHitsObstacle(&game), "a gap near the floor leaves the upper pipe in the way");
}

static void testBirdTouchingEdges(void) {
    Game game = {0};
    clearPipes(&game);
    game.m_bird_y = 150.0f;
    game.m_pipes[0].m_x = (float)BIRD_X;

    game.m_pipes[0].m_gap_y = game.m_bird_y;
    check(!birdHitsObstacle(&game), "sharing the upper pipe's lower edge is safe");
    game.m_pipes[0].m_gap_y = game.m_bird_y + SAFE_GAP_MARGIN;
    check(birdHitsObstacle(&game), "entering the upper pipe rectangle is a collision");

    game.m_pipes[0].m_gap_y = game.m_bird_y + (float)BIRD_HEIGHT - (float)PIPE_GAP_HEIGHT;
    check(!birdHitsObstacle(&game), "sharing the lower pipe's top edge is safe");
    game.m_pipes[0].m_gap_y -= 2.0f;
    check(birdHitsObstacle(&game), "entering the lower pipe rectangle is a collision");

    game.m_pipes[0].m_gap_y = game.m_bird_y + SAFE_GAP_MARGIN;
    game.m_pipes[0].m_x = (float)BIRD_X - (float)PIPE_WIDTH;
    check(!birdHitsObstacle(&game), "sharing a vertical edge is safe");
    game.m_pipes[0].m_x = (float)BIRD_X - (float)PIPE_WIDTH + 0.5f;
    check(birdHitsObstacle(&game), "overlapping horizontally is a collision");
}

static void testBirdOtherPipesAndNoMutation(void) {
    Game game = {0};
    clearPipes(&game);
    game.m_bird_y = 150.0f;
    game.m_pipes[2].m_x = 2000.0f;
    Game before = game;
    check(!birdHitsObstacle(&game), "pipes away from the bird are safe");
    check(memcmp(&before, &game, sizeof game) == 0, "the collision check must not modify the game");

    game.m_pipes[3].m_x = (float)BIRD_X;
    game.m_pipes[3].m_gap_y = game.m_bird_y + SAFE_GAP_MARGIN;
    before = game;
    check(birdHitsObstacle(&game), "any pipe in the way is a collision");
    check(memcmp(&before, &game, sizeof game) == 0,
          "a detected collision still must not modify the game");
}

static void testScoreStrictEdge(void) {
    Game game = {0};
    clearPipes(&game);
    game.m_pipes[0].m_x = (float)BIRD_X - (float)PIPE_WIDTH;
    check(scorePassedPipes(&game) == 0, "a right edge exactly at BIRD_X earns nothing");
    check(!game.m_pipes[0].m_scored, "a touching pipe stays unscored");
    check(game.m_score == 0, "a touching pipe leaves the score alone");

    game.m_pipes[0].m_x -= 0.5f;
    check(scorePassedPipes(&game) == 1, "a right edge just past BIRD_X earns one point");
    check(game.m_pipes[0].m_scored, "a scoring pipe is marked");
    check(game.m_score == 1, "the point is added to the score");
    check(scorePassedPipes(&game) == 0, "a scored pipe cannot score twice");
    check(game.m_score == 1, "a repeated call keeps the score");
}

static void testScoreSeveralAndReturnCount(void) {
    Game game = {0};
    clearPipes(&game);
    game.m_score = TEST_SCORE_START;
    game.m_pipes[0].m_x = 100.0f;
    game.m_pipes[1].m_x = 110.0f;
    game.m_pipes[2].m_x = 150.0f;
    game.m_pipes[2].m_scored = true;
    game.m_best = 7;
    game.m_status = PLAYING;
    const float gapBefore = game.m_pipes[1].m_gap_y;
    int gained = scorePassedPipes(&game);
    check(gained == 2, "two newly passed pipes earn two points");
    check(game.m_score == TEST_SCORE_START + 2, "all new points are added at once");
    check(game.m_best == 7 && game.m_status == PLAYING, "scoring leaves best and status alone");
    check(near(game.m_pipes[1].m_gap_y, gapBefore), "scoring leaves the gap alone");
    check(game.m_pipes[2].m_scored, "an already scored pipe keeps its flag");
}

static void testScoreAfterRecycle(void) {
    Game game = {0};
    clearPipes(&game);
    game.m_pipes[0].m_x = -10.0f;
    game.m_pipes[0].m_scored = true;
    recyclePipe(&game, 0);
    check(!game.m_pipes[0].m_scored, "a recycled slot is unscored again");
    check(scorePassedPipes(&game) == 0, "a recycled slot behind the bird earns nothing");
    game.m_pipes[0].m_x = 100.0f;
    check(scorePassedPipes(&game) == 1, "a reused slot scores once it passes the bird");
}

static void testDemoRun(void) {
    Game game = {0};
    gameInit(&game);
    int previousScore = 0;
    for (int step = 0; step < TEST_STEPS; ++step) {
        gameStep(&game, gameDemoInput(&game), STEP);
        check(game.m_score >= previousScore, "the score never drops during a run");
        previousScore = game.m_score;
        for (int i = 0; i < PIPE_COUNT; ++i) {
            const Pipe *pipe = &game.m_pipes[i];
            check(pipe->m_gap_y >= (float)GAP_LOWEST && pipe->m_gap_y <= GAP_HIGHEST,
                  "every gap stays inside the playable band");
            for (int j = i + 1; j < PIPE_COUNT; ++j) {
                check(!near(pipe->m_x, game.m_pipes[j].m_x),
                      "two slots never share a horizontal position");
            }
        }
        if (g_failures > 0) {
            return;
        }
    }
    check(game.m_score >= DEMO_MIN_SCORE, "the demo pilot passes at least ten pipes");
    printf("demo: score=%d best=%d status=%d\n", game.m_score, game.m_best, (int)game.m_status);
}

int main(void) {
    testRandomDefaults();
    testRandomSeeds();
    testRandomRange();
    testAdvanceMovement();
    testAdvanceZeroTimeAtTheEdge();
    testAdvanceRecyclesAfterMoving();
    testAdvanceSeveralSlots();
    testBirdBounds();
    testBirdGapIsSafe();
    testBirdTouchingEdges();
    testBirdOtherPipesAndNoMutation();
    testScoreStrictEdge();
    testScoreSeveralAndReturnCount();
    testScoreAfterRecycle();
    testDemoRun();
    if (g_failures == 0) {
        printf("All P2 boundary tests passed.\n");
        return 0;
    }
    printf("P2 tests: %d failure(s)\n", g_failures);
    return 1;
}
