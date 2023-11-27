#include "tune.h"
#include <iostream>

double sigmoid(double x, double k)
{
    return 1.0 / (1 + exp(-x * k));
}

double evaluate(const Position& pos, Coeffs coefficients, const EvalParams& params)
{
    double mg = 0, eg = 0;
    for (int i = pos.coeffBegin; i < pos.coeffEnd; i++)
    {
        const auto& coeff = coefficients[i];
        const EvalParam& param = params[coeff.index];
        mg += param.mg * (coeff.white - coeff.black);
        eg += param.eg * (coeff.white - coeff.black);
    }

    return (mg * pos.phase + eg * (1.0 - pos.phase));
}

double findKValue(std::span<const Position> positions, std::span<const Coefficient> coefficients, const EvalParams& params)
{
    constexpr double SEARCH_MAX = 10;
    constexpr int ITERATIONS = 10;

    double start = 0, end = SEARCH_MAX, step = 0.25;
    double bestK = 0, bestError = 1e10;

    for (int i = 0; i < ITERATIONS; i++)
    {
        std::cout << "Iteration: " << i << std::endl;
        std::cout << "Start: " << start + step << " End: " << end + step << " Step: " << step << std::endl;
        for (double curr = start + step; curr < end + step; curr += step)
        {
            double error = calcError(positions, coefficients, curr, params);
            std::cout << "K: " << curr << " Error: " << error << std::endl;
            if (error < bestError)
            {
                std::cout << "New best" << std::endl;
                bestError = error;
                bestK = curr;
            }
        }

        start = bestK - step;
        end = bestK + step;
        step *= 0.1;
    }

    return bestK;
}

double calcError(std::span<const Position> positions, Coeffs coefficients, double kValue, const EvalParams& params)
{
    double error = 0.0;
    for (auto& pos : positions)
    {
        double eval = evaluate(pos, coefficients, params);
        double diff = sigmoid(eval, kValue) - pos.wdl;
        error += diff * diff;
    }
    return error / positions.size();
}

void updateGradient(const Position& pos, Coeffs coefficients, double kValue, const EvalParams& params, std::vector<Gradient>& gradients)
{
    double eval = evaluate(pos, coefficients, params);
    double wdl = sigmoid(eval, kValue);
    double gradientBase = (wdl - pos.wdl) * (wdl * (1 - wdl));
    double mgBase = gradientBase * pos.phase;
    double egBase = gradientBase - mgBase;
    for (int i = pos.coeffBegin; i < pos.coeffEnd; i++)
    {
        const auto& coeff = coefficients[i];
        gradients[coeff.index].mg += (coeff.white - coeff.black) * mgBase;
        gradients[coeff.index].eg += (coeff.white - coeff.black) * egBase;
    }
}

void computeGradient(std::span<const Position> positions, Coeffs coefficients, double kValue, const EvalParams& params, std::vector<Gradient>& gradients)
{
    std::fill(gradients.begin(), gradients.end(), Gradient{0, 0});

    for (const auto& pos : positions)
        updateGradient(pos, coefficients, kValue, params, gradients);

    for (auto& grad : gradients)
    {
        // technically, this is actually the gradient multiplied by 0.5
        grad.mg = grad.mg * kValue / positions.size();
        grad.eg = grad.eg * kValue / positions.size();
    }
}
