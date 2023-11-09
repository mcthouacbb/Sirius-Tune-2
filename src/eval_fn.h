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
    std::pair<size_t, size_t> getCoefficients(const chess::Board& board);
    static EvalParams getInitialParams();
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

    std::vector<Coefficient>& m_Coefficients;
    int m_TraceIdx;
};
