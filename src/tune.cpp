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
