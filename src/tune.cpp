#include "tune.h"

double sigmoid(double x, double k)
{
    return 1.0 / (1 + exp(-x * k));
}

double linearEval(const Position& pos, const std::vector<EvalParam>& params)
{
    double mg = 0, eg = 0;
    for (auto coeff : pos.coefficients)
    {
        const EvalParam& param = params[coeff.index];
        mg += param.mg * (coeff.white - coeff.black);
        eg += param.eg * (coeff.white - coeff.black);
    }

    return (mg * pos.phase + eg * (1.0 - pos.phase));
}

double calcError(std::span<const Position> positions, double kValue, const std::vector<EvalParam>& params)
{
    double error = 0.0;
    for (auto& pos : positions)
    {
        double eval = linearEval(pos, params);
        double diff = sigmoid(eval, kValue) - pos.wdl;
        error += diff * diff;
    }
    return error / positions.size();
}
