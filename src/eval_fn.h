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
    std::pair<size_t, size_t> getCoefficients(const Board& board);
    static EvalParams getInitialParams();
    static EvalParams getMaterialParams();
    static void printEvalParams(const EvalParams& params, std::ostream& os);
    static void printEvalParamsExtracted(const EvalParams& params, std::ostream& os);
private:
    template<typename T>
    void addCoefficient(const T& trace, CoeffType type)
    {
        if ((trace[0] != 0 || trace[1] != 0) &&
            (type != CoeffType::LINEAR || trace[0] - trace[1] != 0))
            m_Coefficients.push_back({
                static_cast<int16_t>(m_TraceIdx),
                static_cast<int16_t>(trace[0]),
                static_cast<int16_t>(trace[1]),
                type
            });
        m_TraceIdx++;
    }

    template<typename T>
    void addCoefficientArray(const T& trace, CoeffType type)
    {
        for (auto traceElem : trace)
            addCoefficient(traceElem, type);
    }

    template<typename T>
    void addCoefficientArray2D(const T& trace, CoeffType type)
    {
        for (auto& traceElem : trace)
            addCoefficientArray(traceElem, type);
    }

    std::vector<Coefficient>& m_Coefficients;
    int m_TraceIdx;
};
