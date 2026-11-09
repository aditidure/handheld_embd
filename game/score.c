#include "score.h"

void score_init(Score *score)
{
    score->current_score = 0;
    score->best_score = 0;
}

void score_add(Score *score, int points)
{
    if (points > 0) {
        score->current_score += points;
    }
}

void score_reset(Score *score)
{
    score->current_score = 0;
}

int score_get_current(const Score *score)
{
    return score->current_score;
}

int score_get_best(const Score *score)
{
    return score->best_score;
}

void score_set_best(Score *score, int best)
{
    score->best_score = (best > 0) ? best : 0;
}

void score_update_best(Score *score)
{
    if (score->current_score > score->best_score) {
        score->best_score = score->current_score;
    }
}