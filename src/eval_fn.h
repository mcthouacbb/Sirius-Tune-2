#pragma once

#include "dataset.h"
#include "tune.h"
#include "sirius/board.h"
#include <algorithm>

class EvalFn
{
public:
    EvalFn(std::vector<Coefficient>& coefficients);

    void reset();
    std::tuple<size_t, size_t, double> getCoefficients(const Board& board);
    static EvalParams getInitialParams();
    static EvalParams getMaterialParams();
    static void printEvalParams(const EvalParams& params, std::ostream& os);
    static void printEvalParamsExtracted(const EvalParams& params, std::ostream& os);
private:
    template<typename T>
    void addCoefficient(const T& trace)
    {
        if (trace[0] - trace[1] != 0)
            m_Coefficients.push_back({static_cast<int16_t>(m_TraceIdx), static_cast<int16_t>(trace[0]), static_cast<int16_t>(trace[1])});
        m_TraceIdx++;
    }

    template<typename T>
    void addCoefficientArray(const T& trace)
    {
        for (auto traceElem : trace)
            addCoefficient(traceElem);
    }

    template<typename T>
    void addCoefficientArray2D(const T& trace)
    {
        for (auto& traceElem : trace)
            addCoefficientArray(traceElem);
    }

    template<typename T>
    void addCoefficientArray3D(const T& trace)
    {
        for (auto& traceElem : trace)
            addCoefficientArray2D(traceElem);
    }

    std::vector<Coefficient>& m_Coefficients;
    int m_TraceIdx;
};
