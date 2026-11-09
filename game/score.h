#ifndef SCORE_H
#define SCORE_H

typedef struct {
    int current_score;
    int best_score;
} Score;

void score_init(Score *score);

void score_add(Score *score, int points);

void score_reset(Score *score);

int score_get_current(const Score *score);

int score_get_best(const Score *score);

void score_update_best(Score *score);

// Set the best score directly (e.g. after loading it from flash).
void score_set_best(Score *score, int best);

#endif