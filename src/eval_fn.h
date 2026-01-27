#pragma once

#include "dataset.h"
#include "sirius/board.h"
#include "tune.h"
#include <algorithm>

class EvalFn
{
public:
    EvalFn(std::vector<Coefficient>& coefficients);

    void reset();
    std::tuple<size_t, size_t, double> getCoefficients(const Board& board);
    static EvalParams getInitialParams();
    static EvalParams getMaterialParams();
    static EvalParams getKParams();
    static void printEvalParams(const EvalParams& params, std::ostream& os);
    static void printEvalParamsExtracted(const EvalParams& params, std::ostream& os);

private:
    template<typename T>
    void addCoefficient(const T& trace, ParamType type)
    {
        if ((type == ParamType::NORMAL && trace[0] - trace[1] != 0)
            || (type == ParamType::COMPLEXITY && trace[0] != 0)
            || (type == ParamType::SAFETY && (trace[0] != 0 || trace[1] != 0)))
            m_Coefficients.push_back({static_cast<i16>(m_TraceIdx), static_cast<i16>(trace[0]),
                static_cast<i16>(trace[1])});
        m_TraceIdx++;
    }

    template<typename T>
    void addCoefficientArray(const T& trace, ParamType type)
    {
        for (auto traceElem : trace)
            addCoefficient(traceElem, type);
    }

    template<typename T>
    void addCoefficientArray2D(const T& trace, ParamType type)
    {
        for (auto& traceElem : trace)
            addCoefficientArray(traceElem, type);
    }

    template<typename T>
    void addCoefficientArray3D(const T& trace, ParamType type)
    {
        for (auto& traceElem : trace)
            addCoefficientArray2D(traceElem, type);
    }

    std::vector<Coefficient>& m_Coefficients;
    int m_TraceIdx;
};
