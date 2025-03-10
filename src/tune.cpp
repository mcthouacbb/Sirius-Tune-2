#include "tune.h"
#include "eval_fn.h"
#include "thread_pool.h"
#include "settings.h"
#include <iostream>
#include <chrono>

double sigmoid(double x, double k)
{
    return 1.0 / (1 + exp(-x * k));
}

struct EvalTrace
{
    struct TraceElem
    {
        double mg;
        double eg;
    };
    TraceElem normal;
    ColorArray<TraceElem> rawSafety;
    TraceElem nonComplexity;
    TraceElem complexity;
};

double evaluate(const Position& pos, Coeffs coefficients, const EvalParams& params, EvalTrace& trace)
{
    for (int i = pos.coeffBegin; i < pos.coeffEnd; i++)
    {
        const auto& coeff = coefficients[i];
        const EvalParam& param = params[coeff.index];
        if (param.type == ParamType::NORMAL)
        {
            trace.normal.mg += param.mg * (coeff.white - coeff.black);
            trace.normal.eg += param.eg * (coeff.white - coeff.black);
        }
        else if (param.type == ParamType::COMPLEXITY)
        {
            // complexity has no white/black separation, only white part is used
            trace.complexity.eg += param.eg * coeff.white;
        }
        else if (param.type == ParamType::SAFETY)
        {
            trace.rawSafety[Color::WHITE].mg += param.mg * coeff.white;
            trace.rawSafety[Color::WHITE].eg += param.eg * coeff.white;
            trace.rawSafety[Color::BLACK].mg += param.mg * coeff.black;
            trace.rawSafety[Color::BLACK].eg += param.eg * coeff.black;
        }
    }

    double mg = 0, eg = 0;
    mg += trace.normal.mg;
    eg += trace.normal.eg;
    mg += trace.rawSafety[Color::WHITE].mg * params.safetyScales[pos.safetyScales[static_cast<int>(Color::WHITE)]].mg / 128.0;
    mg -= trace.rawSafety[Color::BLACK].mg * params.safetyScales[pos.safetyScales[static_cast<int>(Color::BLACK)]].mg / 128.0;
    eg += trace.rawSafety[Color::WHITE].eg * params.safetyScales[pos.safetyScales[static_cast<int>(Color::WHITE)]].mg / 128.0;
    eg -= trace.rawSafety[Color::BLACK].eg * params.safetyScales[pos.safetyScales[static_cast<int>(Color::BLACK)]].mg / 128.0;
    trace.nonComplexity.mg = mg;
    trace.nonComplexity.eg = eg;
    eg += ((eg > 0) - (eg < 0)) * std::max(-std::abs(eg), trace.complexity.eg);

    return (mg * pos.phase + eg * (1.0 - pos.phase));
}

double evaluate(const Position& pos, Coeffs coefficients, const EvalParams& params)
{
    EvalTrace trace = {};
    return evaluate(pos, coefficients, params, trace);
}

double findKValue(ThreadPool& threadPool, std::span<const Position> positions, Coeffs coefficients, const EvalParams& params)
{
    constexpr double SEARCH_MAX = 1;
    constexpr int ITERATIONS = 7;

    double start = 0, end = SEARCH_MAX, step = 0.025;
    double bestK = 0, bestError = 1e10;

    for (int i = 0; i < ITERATIONS; i++)
    {
        std::cout << "Iteration: " << i << std::endl;
        std::cout << "Start: " << start + step << " End: " << end + step << " Step: " << step << std::endl;
        for (double curr = start + step; curr < end + step; curr += step)
        {
            double error = calcError(threadPool, positions, coefficients, curr, params);
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

double calcError(ThreadPool& threadPool, std::span<const Position> positions, Coeffs coefficients, double kValue, const EvalParams& params)
{
    std::vector<double> threadErrors(threadPool.concurrency());
    for (uint32_t threadID = 0; threadID < threadPool.concurrency(); threadID++)
    {
        size_t beginIdx = positions.size() * threadID / threadPool.concurrency();
        size_t endIdx = positions.size() * (threadID + 1) / threadPool.concurrency();
        std::span<const Position> threadPositions = positions.subspan(beginIdx, endIdx - beginIdx);
        threadPool.addTask([threadID, &threadErrors, threadPositions, coefficients, kValue, &params]()
        {
            double error = 0.0;
            for (auto& pos : threadPositions)
            {
                double eval = evaluate(pos, coefficients, params);
                double diff = sigmoid(eval, kValue) - pos.wdl;
                error += diff * diff;
            }
            threadErrors[threadID] = error;
        });
    }
    threadPool.wait();
    double error = 0.0;
    for (uint32_t threadID = 0; threadID < threadPool.concurrency(); threadID++)
        error += threadErrors[threadID];
    return error / static_cast<double>(positions.size());
}

void updateGradient(const Position& pos, Coeffs coefficients, double kValue, const EvalParams& params, std::vector<Gradient>& gradients)
{
    EvalTrace trace = {};
    double eval = evaluate(pos, coefficients, params, trace);
    double wdl = sigmoid(eval, kValue);
    double gradientBase = (wdl - pos.wdl) * (wdl * (1 - wdl));
    double mgBase = gradientBase * pos.phase;
    double egBase = gradientBase - mgBase;
    if (trace.complexity.mg >= -std::abs(trace.nonComplexity.mg))
    {
        // HACK for now: Make safetyScales[2] fixed to 128 / 128 = 1
        if (pos.safetyScales[static_cast<int>(Color::WHITE)] != 2)
        {
            gradients[params.linear.size() + pos.safetyScales[static_cast<int>(Color::WHITE)]].mg +=
                trace.rawSafety[Color::WHITE].mg * mgBase +
                trace.rawSafety[Color::WHITE].eg * egBase;
        }
        if (pos.safetyScales[static_cast<int>(Color::BLACK)] != 2)
        {
            gradients[params.linear.size() + pos.safetyScales[static_cast<int>(Color::BLACK)]].mg -=
                trace.rawSafety[Color::BLACK].mg * mgBase +
                trace.rawSafety[Color::BLACK].eg * egBase;
        }
    }
    for (int i = pos.coeffBegin; i < pos.coeffEnd; i++)
    {
        const auto& coeff = coefficients[i];
        ParamType type = params[coeff.index].type;
        if (type == ParamType::NORMAL)
        {
            if (trace.complexity.mg >= -std::abs(trace.nonComplexity.mg))
                gradients[coeff.index].mg += (coeff.white - coeff.black) * mgBase;
            if (trace.complexity.eg >= -std::abs(trace.nonComplexity.eg))
                gradients[coeff.index].eg += (coeff.white - coeff.black) * egBase * pos.egScale;
        }
        else if (type == ParamType::SAFETY)
        {
            if (trace.complexity.mg >= -std::abs(trace.nonComplexity.mg))
            {
                gradients[coeff.index].mg += coeff.white * mgBase * params.safetyScales[pos.safetyScales[static_cast<int>(Color::WHITE)]].mg;
                gradients[coeff.index].mg -= coeff.black * mgBase * params.safetyScales[pos.safetyScales[static_cast<int>(Color::BLACK)]].mg;
            }
            if (trace.complexity.eg >= -std::abs(trace.nonComplexity.eg))
            {
                gradients[coeff.index].eg += coeff.white * egBase * pos.egScale * params.safetyScales[pos.safetyScales[static_cast<int>(Color::WHITE)]].mg;
                gradients[coeff.index].eg -= coeff.black * egBase * pos.egScale * params.safetyScales[pos.safetyScales[static_cast<int>(Color::BLACK)]].mg;
            }
        }
        else if (type == ParamType::COMPLEXITY)
        {
            if (trace.complexity.eg >= -std::abs(trace.nonComplexity.eg))
                gradients[coeff.index].eg += egBase * coeff.white * pos.egScale * ((trace.normal.eg > 0) - (trace.normal.eg < 0));
        }
    }
}

void computeGradient(ThreadPool& threadPool, std::span<const Position> positions, Coeffs coefficients, double kValue, const EvalParams& params, std::vector<Gradient>& gradients)
{
    std::fill(gradients.begin(), gradients.end(), Gradient{0, 0});

    std::vector<std::vector<Gradient>> threadGradients(threadPool.concurrency(), gradients);

    for (uint32_t threadID = 0; threadID < threadPool.concurrency(); threadID++)
    {
        size_t beginIdx = positions.size() * threadID / threadPool.concurrency();
        size_t endIdx = positions.size() * (threadID + 1) / threadPool.concurrency();
        std::span<const Position> threadPositions = positions.subspan(beginIdx, endIdx - beginIdx);
        threadPool.addTask([threadID, &threadGradients, threadPositions, coefficients, kValue, &params]()
        {
            for (const auto& pos : threadPositions)
                updateGradient(pos, coefficients, kValue, params, threadGradients[threadID]);
        });
    }

    threadPool.wait();

    for (uint32_t i = 0; i < gradients.size(); i++)
    {
        Gradient grad = {};
        for (uint32_t threadID = 0; threadID < threadPool.concurrency(); threadID++)
        {
            grad.mg += threadGradients[threadID][i].mg;
            grad.eg += threadGradients[threadID][i].eg;
        }
        // technically, this is actually the gradient multiplied by 0.5
        grad.mg = grad.mg * kValue / positions.size();
        grad.eg = grad.eg * kValue / positions.size();
        gradients[i] = grad;
    }
}

EvalParams tune(const Dataset& dataset, std::ofstream& outFile)
{
    ThreadPool threadPool(TUNE_THREADS);
    EvalParams params = EvalFn::getInitialParams();
    double kValue = TUNE_K <= 0 ? findKValue(threadPool, dataset.positions, dataset.allCoefficients, EvalFn::getKParams()) : TUNE_K;
    std::cout << "Final k value: " << kValue << std::endl;
    outFile << "Final k value: " << kValue << std::endl;
    if constexpr (TUNE_FROM_ZERO)
        for (auto& param : params.linear)
            param.mg = param.eg = 0;
    else if constexpr (TUNE_FROM_MATERIAL)
        params = EvalFn::getMaterialParams();

    constexpr double LR = TUNE_LR;

    constexpr double BETA1 = 0.9, BETA2 = 0.999;
    constexpr double EPSILON = 1e-8;

    std::vector<Gradient> momentum(params.totalSize(), {0, 0});
    std::vector<Gradient> velocity(params.totalSize(), {0, 0});
    std::vector<Gradient> gradient(params.totalSize(), {0, 0});

    auto t1 = std::chrono::steady_clock::now();
    auto startTime = t1;

    for (int epoch = 1; epoch <= TUNE_MAX_EPOCHS; epoch++)
    {
        for (int batch = 0; batch < dataset.positions.size() / BATCH_SIZE; batch++)
        {
            std::span<const Position> batchPositions = {
                dataset.positions.begin() + batch * BATCH_SIZE,
                std::min<size_t>(BATCH_SIZE, dataset.positions.size() - batch * BATCH_SIZE)
            };
            computeGradient(threadPool, batchPositions, dataset.allCoefficients, kValue, params, gradient);

            for (int i = 0; i < gradient.size(); i++)
            {
                momentum[i].mg = BETA1 * momentum[i].mg + (1 - BETA1) * gradient[i].mg;
                momentum[i].eg = BETA1 * momentum[i].eg + (1 - BETA1) * gradient[i].eg;

                velocity[i].mg = BETA2 * velocity[i].mg + (1 - BETA2) * gradient[i].mg * gradient[i].mg;
                velocity[i].eg = BETA2 * velocity[i].eg + (1 - BETA2) * gradient[i].eg * gradient[i].eg;

                params[i].mg -= LR * momentum[i].mg / (std::sqrt(velocity[i].mg) + EPSILON);
                params[i].eg -= LR * momentum[i].eg / (std::sqrt(velocity[i].eg) + EPSILON);
            }
        }
        if (epoch % 10 == 0)
        {
            double error = calcError(threadPool, dataset.positions, dataset.allCoefficients, kValue, params);
            std::cout << "Epoch: " << epoch << std::endl;
            std::cout << "Error: " << error << std::endl;
            outFile << "Epoch: " << epoch << std::endl;
            outFile << "Error: " << error << std::endl;

            auto t2 = std::chrono::steady_clock::now();
            auto totalTime = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - startTime).count();
            std::cout << "Epochs/s (total): " << static_cast<double>(epoch) / totalTime << std::endl;
            std::cout << "Epochs/s (avg of last 100): " << 100.0f / std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1).count() << std::endl;
            std::cout << "Total time: " << totalTime << std::endl;
            outFile << "Epochs/s (total): " << static_cast<double>(epoch) / totalTime << std::endl;
            outFile << "Epochs/s (avg of last 100): " << 100.0f / std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1).count() << std::endl;
            outFile << "Total time: " << totalTime << std::endl;

            t1 = t2;
            EvalFn::printEvalParams(params, std::cout);
            std::cout << std::endl;/*

            EvalParams extracted = params;
            for (auto& param : extracted)
            {
                param.mg = std::round(param.mg);
                param.eg = std::round(param.eg);
            }*/
            //EvalFn::printEvalParamsExtracted(extracted, outFile);
            EvalFn::printEvalParamsExtracted(params, outFile);
            outFile << std::endl;
        }
    }
    return params;
}
