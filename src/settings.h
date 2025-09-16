#pragma once

constexpr int TUNE_THREADS = 4;
constexpr int TUNE_MAX_EPOCHS = 900;
constexpr bool TUNE_FROM_ZERO = false;
constexpr bool TUNE_FROM_MATERIAL = true;
constexpr int BATCH_SIZE = 16384;
constexpr double WDL_LAMBDA = 0.75;
constexpr float TUNE_LR = 0.02;
constexpr float TUNE_K = 0.0;

static_assert(TUNE_MAX_EPOCHS % 100 == 0 && TUNE_MAX_EPOCHS > 0,
    "TUNE_MAX_EPOCHS must be divisible by 100 and greater than 0");
static_assert(!TUNE_FROM_ZERO || !TUNE_FROM_MATERIAL, "Cannot tune from zero and material values");
