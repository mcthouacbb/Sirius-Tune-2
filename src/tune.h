#pragma once

#include <cstdint>
#include <cmath>
#include <span>
#include "dataset.h"

struct EvalParam
{
    double mg;
    double eg;
};

double calcError(std::span<const Position> positions, double kValue, const std::vector<EvalParam>& params);
