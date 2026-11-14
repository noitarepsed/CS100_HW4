#include "flappy.h"

/* A pipe pair is done with the view once its right edge reaches the left
 * boundary; the slot may then be recycled behind the rightmost pipe. */
static bool pipeLeftTheView(const Pipe *pipe) { return pipe->m_x + (float)PIPE_WIDTH <= 0.0f; }

/* The upper pipe hangs from the ceiling down to the gap. */
static Box upperPipeBox(const Pipe *pipe) {
    return (Box){pipe->m_x, 0.0f, (float)PIPE_WIDTH, pipe->m_gap_y};
}

/* The lower pipe starts right below the gap and reaches the ground. */
static Box lowerPipeBox(const Pipe *pipe) {
    const float lowerTop = pipe->m_gap_y + (float)PIPE_GAP_HEIGHT;
    const float lowerHeight = (float)FLOOR_Y - lowerTop;
    return (Box){pipe->m_x, lowerTop, (float)PIPE_WIDTH, lowerHeight};
}

void advancePipes(Game *game, float dt) {
    const float shift = PIPE_SPEED * dt;

    /* Every slot moves first: recyclePipe looks at the current positions. */
    for (int i = 0; i < PIPE_COUNT; ++i) {
        game->m_pipes[i].m_x -= shift;
    }
    for (int i = 0; i < PIPE_COUNT; ++i) {
        if (pipeLeftTheView(&game->m_pipes[i])) {
            recyclePipe(game, i);
        }
    }
}

bool birdHitsObstacle(const Game *game) {
    const Box bird = gameBirdBox(game);

    /* Exact ceiling or floor contact is safe; crossing them is not. */
    if (bird.m_y < 0.0f || bird.m_y + bird.m_height > (float)FLOOR_Y) {
        return true;
    }
    for (int i = 0; i < PIPE_COUNT; ++i) {
        const Pipe *pipe = &game->m_pipes[i];
        if (boxesOverlap(bird, upperPipeBox(pipe)) || boxesOverlap(bird, lowerPipeBox(pipe))) {
            return true;
        }
    }
    return false;
}

int scorePassedPipes(Game *game) {
    int gained = 0;

    for (int i = 0; i < PIPE_COUNT; ++i) {
        Pipe *pipe = &game->m_pipes[i];

        /* Strictly left: a right edge exactly at BIRD_X has not passed yet. */
        if (!pipe->m_scored && pipe->m_x + (float)PIPE_WIDTH < (float)BIRD_X) {
            pipe->m_scored = true;
            ++gained;
        }
    }
    game->m_score += gained;
    return gained;
}

unsigned int nextRandom(unsigned int seed) {
    static unsigned int state = RANDOM_INITIAL_SEED;

    if (seed > 0) {
        state = seed % RANDOM_MODULUS;
    }
    state = (RANDOM_MULTIPLIER * state + RANDOM_INCREMENT) % RANDOM_MODULUS;
    return state;
}
