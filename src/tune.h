#pragma once

#include <cstdint>
#include <cmath>
#include <span>
#include <fstream>
#include "dataset.h"
#include "thread_pool.h"

enum class ParamType
{
    NORMAL,
    COMPLEXITY
};

struct EvalParam
{
    ParamType type;
    double mg;
    double eg;
};
// hack lol
struct Gradient
{
    double mg;
    double eg;
};

using Coeffs = std::span<const Coefficient>;
using EvalParams = std::vector<EvalParam>;

double findKValue(ThreadPool& threadPool, std::span<const Position> positions, Coeffs coefficients, const EvalParams& params);

double calcError(ThreadPool& threadPool, std::span<const Position> positions, Coeffs coefficients, double kValue, const EvalParams& params);
void computeGradient(ThreadPool& threadPool, std::span<const Position> positions, Coeffs coefficients, double kValue, const EvalParams& params, std::vector<Gradient>& gradients);

EvalParams tune(const Dataset& dataset, std::ofstream& outFile);
