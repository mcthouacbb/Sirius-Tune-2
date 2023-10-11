#pragma once

#include "dataset.h"
#include "tune.h"
#include "chess.hpp"
#include <algorithm>

class EvalFn
{
public:
    EvalFn(std::vector<Coefficient>& coefficients);

    void reset();
    std::span<Coefficient> getCoefficients(const chess::Board& board);
    static std::vector<EvalParam> getInitialParams();
private:
    template<typename T>
    void addCoefficient(const T& trace)
    {
        if (trace[0] - trace[1] != 0)
            m_Coefficients.push_back({m_TraceIdx, trace[0], trace[1]});
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

    std::vector<Coefficient>& m_Coefficients;
    int m_TraceIdx;
};
