#pragma once

#include <cstdint>
#include <cmath>
#include <span>
#include <fstream>
#include <array>
#include "dataset.h"
#include "thread_pool.h"

enum class ParamType
{
    NORMAL,
    COMPLEXITY,
    SAFETY
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
struct EvalParams
{
    EvalParam& operator[](size_t idx)
    {
        if (idx >= linear.size())
            return safetyScales[idx - linear.size()];
        return linear[idx];
    }

    const EvalParam& operator[](size_t idx) const
    {
        if (idx >= linear.size())
            return safetyScales[idx - linear.size()];
        return linear[idx];
    }

    size_t totalSize() const
    {
        return linear.size() + safetyScales.size();
    }

    std::vector<EvalParam> linear;
    std::array<EvalParam, 6> safetyScales;
};

double findKValue(ThreadPool& threadPool, std::span<const Position> positions, Coeffs coefficients, const EvalParams& params);

double calcError(ThreadPool& threadPool, std::span<const Position> positions, Coeffs coefficients, double kValue, const EvalParams& params);
void computeGradient(ThreadPool& threadPool, std::span<const Position> positions, Coeffs coefficients, double kValue, const EvalParams& params, std::vector<Gradient>& gradients);

EvalParams tune(const Dataset& dataset, std::ofstream& outFile);
