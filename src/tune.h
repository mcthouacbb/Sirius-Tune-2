#pragma once

#include <cstdint>
#include <cmath>
#include <span>
#include "dataset.h"
#include "thread_pool.h"

struct EvalParam
{
    double mg;
    double eg;
};
// hack lol
using Gradient = EvalParam;

using Coeffs = std::span<const Coefficient>;
using EvalParams = std::vector<EvalParam>;

double findKValue(ThreadPool& threadPool, std::span<const Position> positions, Coeffs coefficients, const EvalParams& params);

double calcError(ThreadPool& threadPool, std::span<const Position> positions, Coeffs coefficients, double kValue, const EvalParams& params);
void computeGradient(ThreadPool& threadPool, std::span<const Position> positions, Coeffs coefficients, double kValue, const EvalParams& params, std::vector<Gradient>& gradients);

EvalParams tune(const Dataset& dataset);
