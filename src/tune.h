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
// hack lol
using Gradient = EvalParam;

using Coeffs = std::span<const Coefficient>;
using EvalParams = std::vector<EvalParam>;

double calcError(std::span<const Position> positions, std::span<const Coefficient> coefficients, double kValue, const EvalParams& params);
void computeGradient(std::span<const Position> positions, Coeffs coefficients, double kValue, const EvalParams& params, std::vector<Gradient>& gradients);
