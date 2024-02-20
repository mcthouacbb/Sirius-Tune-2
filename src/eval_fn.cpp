#include "eval_fn.h"

#include <sstream>
#include <iomanip>

using TraceElem = std::array<int, 2>;

struct Trace
{
    TraceElem psqt[6][64];
    TraceElem bishopPair;
};

Trace getTrace(const chess::Board& board)
{
    Trace trace = {};

    for (int sq = 0; sq < 64; sq++)
    {
        chess::Piece pce = board.at(static_cast<chess::Square>(sq));
        if (pce == chess::Piece::NONE)
            continue;

        chess::PieceType type = pce.type();
        chess::Color color = pce.color();

        // flip if white
        int square = sq ^ (color == chess::Color::WHITE ? 0b111000 : 0);

        trace.psqt[static_cast<int>(type)][square][static_cast<int>(color)]++;
    }

    if (board.pieces(chess::PieceType::BISHOP, chess::Color::WHITE).count() >= 2)
        trace.bishopPair[static_cast<int>(chess::Color::WHITE)] = 1;
    if (board.pieces(chess::PieceType::BISHOP, chess::Color::BLACK).count() >= 2)
        trace.bishopPair[static_cast<int>(chess::Color::BLACK)] = 1;

    return trace;
}

EvalFn::EvalFn(std::vector<Coefficient>& coefficients)
    : m_Coefficients(coefficients)
{

}

void EvalFn::reset()
{
    m_TraceIdx = 0;
}

std::pair<size_t, size_t> EvalFn::getCoefficients(const chess::Board& board)
{
    reset();
    size_t pos = m_Coefficients.size();
    Trace trace = getTrace(board);
    addCoefficientArray2D(trace.psqt);
    addCoefficient(trace.bishopPair);
    return {pos, m_Coefficients.size()};
}

using InitialParam = std::array<int, 2>;

struct InitialParams
{
    InitialParam material[6];
    InitialParam psqt[6][64];
    InitialParam bishopPair;
};

#define S(mg, eg) {mg, eg}

constexpr InitialParams DEFAULT_PARAMS = {
    {
        S(63, 119), S(268, 339), S(283, 355), S(382, 634), S(781, 1193), S(0, 0)
    },
        {
            {
            S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
            S(  73,  164), S(  95,  156), S(  73,  157), S( 102,  109), S(  86,  104), S(  69,  115), S(   3,  159), S( -22,  173),
            S(  -4,  105), S(   9,  113), S(  41,   79), S(  46,   58), S(  49,   50), S(  71,   35), S(  52,   81), S(  10,   79),
            S( -20,   35), S(   5,   25), S(   8,    6), S(  10,   -3), S(  31,  -11), S(  23,   -9), S(  26,   10), S(   3,   11),
            S( -30,   11), S(  -3,    9), S(  -4,   -9), S(  12,  -11), S(  12,  -14), S(   5,  -12), S(  13,   -1), S(  -9,   -8),
            S( -31,    5), S(  -7,    7), S(  -7,  -10), S(  -6,    2), S(   8,   -5), S(  -2,   -8), S(  27,   -3), S(  -3,  -12),
            S( -31,   10), S(  -7,   12), S( -11,   -2), S( -20,    4), S(  -1,    9), S(  13,   -3), S(  36,   -3), S( -10,  -11),
            S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
        },
        {
            S(-140,  -73), S(-108,  -16), S( -44,   -3), S( -11,  -11), S(  20,   -7), S( -36,  -31), S( -91,  -10), S( -84,  -97),
            S( -10,  -23), S(   6,   -2), S(  32,    6), S(  47,    5), S(  31,   -3), S(  93,  -18), S(   5,   -6), S(  28,  -39),
            S(   3,   -7), S(  37,    8), S(  54,   24), S(  65,   25), S( 101,    9), S( 102,    4), S(  60,   -3), S(  29,  -18),
            S(   0,    5), S(  13,   25), S(  37,   35), S(  57,   38), S(  40,   39), S(  64,   33), S(  23,   24), S(  33,   -3),
            S( -13,    6), S(   2,   15), S(  16,   38), S(  17,   38), S(  26,   41), S(  21,   31), S(  19,   16), S(  -3,   -3),
            S( -32,   -9), S( -10,    9), S(   4,   19), S(   6,   32), S(  17,   31), S(   8,   15), S(  11,    4), S( -16,   -8),
            S( -45,  -18), S( -33,   -3), S( -17,    7), S(  -5,   11), S(  -4,   10), S(  -2,    4), S( -15,  -12), S( -18,   -9),
            S( -87,  -25), S( -35,  -37), S( -47,   -9), S( -33,   -5), S( -29,   -4), S( -16,  -16), S( -32,  -30), S( -58,  -39),
        },
        {
            S( -25,   -7), S( -42,    1), S( -33,    0), S( -75,   12), S( -62,    7), S( -45,   -3), S( -18,   -7), S( -52,  -12),
            S(  -9,  -23), S(  14,   -3), S(   7,    1), S(  -9,    5), S(  20,   -6), S(  19,   -5), S(  11,    1), S(   1,  -24),
            S(   0,    5), S(  24,    2), S(  23,   13), S(  46,    0), S(  33,    6), S(  64,    7), S(  42,    0), S(  29,   -1),
            S(  -8,    1), S(   5,   18), S(  27,   13), S(  37,   27), S(  35,   17), S(  31,   17), S(   6,   14), S(  -7,    1),
            S( -15,   -3), S(  -2,   15), S(   4,   23), S(  24,   19), S(  21,   20), S(   6,   17), S(  -1,   12), S(  -7,  -12),
            S(  -4,   -4), S(   2,    8), S(   3,   14), S(   6,   15), S(   7,   19), S(   2,   16), S(   4,   -1), S(   8,  -13),
            S(  -2,   -8), S(  -1,   -8), S(  10,  -10), S( -11,    5), S(  -4,    6), S(   9,   -5), S(  15,   -2), S(   2,  -28),
            S( -24,  -26), S(  -3,   -8), S( -19,  -28), S( -27,   -7), S( -23,  -10), S( -24,   -9), S(   0,  -22), S( -14,  -39),
        },
        {
            S(  30,    8), S(  19,   16), S(  26,   25), S(  32,   21), S(  49,   12), S(  67,    1), S(  47,    4), S(  69,   -2),
            S(  10,    9), S(  10,   20), S(  28,   24), S(  47,   16), S(  34,   16), S(  63,    2), S(  50,   -3), S(  80,  -16),
            S( -10,    9), S(  11,   11), S(  12,   13), S(  15,   11), S(  43,   -2), S(  44,   -8), S(  81,  -16), S(  59,  -21),
            S( -26,   11), S( -11,    9), S( -10,   18), S(  -2,   15), S(   4,    0), S(   6,   -6), S(  14,  -10), S(  16,  -16),
            S( -44,    4), S( -41,    8), S( -32,   10), S( -19,    9), S( -19,    5), S( -34,    3), S( -11,  -10), S( -19,  -15),
            S( -50,    0), S( -41,   -1), S( -33,   -1), S( -34,    4), S( -28,   -1), S( -30,   -9), S(   4,  -29), S( -18,  -28),
            S( -53,   -6), S( -41,   -2), S( -27,   -1), S( -30,    1), S( -26,   -8), S( -24,  -12), S(  -6,  -22), S( -36,  -16),
            S( -35,  -10), S( -33,   -1), S( -24,    7), S( -19,    5), S( -15,   -2), S( -25,   -7), S( -10,  -11), S( -33,  -19),
        },
        {
            S( -34,    0), S( -29,   15), S(   2,   31), S(  34,   18), S(  33,   16), S(  39,    8), S(  58,  -36), S(   6,   -6),
            S(   2,  -34), S( -20,    9), S( -14,   43), S( -22,   60), S( -16,   77), S(  21,   36), S(   1,   20), S(  44,   -5),
            S(   2,  -23), S(  -1,   -5), S(  -3,   36), S(  13,   38), S(  18,   52), S(  58,   32), S(  60,   -5), S(  58,  -18),
            S( -15,  -13), S( -11,   10), S(  -7,   25), S(  -7,   47), S(  -6,   61), S(   8,   46), S(   7,   32), S(  13,   10),
            S( -13,  -15), S( -15,   14), S( -16,   22), S(  -8,   42), S(  -9,   39), S( -10,   31), S(   1,   11), S(   4,   -1),
            S( -16,  -26), S(  -8,  -10), S( -14,   13), S( -14,    9), S( -11,   13), S(  -4,    6), S(   8,  -16), S(   2,  -29),
            S( -17,  -31), S( -12,  -27), S(  -2,  -30), S(  -2,  -21), S(  -4,  -18), S(   5,  -43), S(  11,  -72), S(  21, -101),
            S( -20,  -36), S( -30,  -30), S( -23,  -26), S(  -7,  -35), S( -16,  -31), S( -30,  -31), S(  -6,  -62), S( -15,  -61),
        },
        {
            S(  63, -103), S(  39,  -53), S(  73,  -44), S( -68,    6), S( -14,  -14), S(  36,  -11), S(  89,  -20), S( 194, -125),
            S( -56,  -10), S( -14,   18), S( -57,   31), S(  48,   12), S(  -3,   33), S(   1,   44), S(  41,   34), S(  18,    3),
            S( -74,    5), S(  27,   23), S( -39,   42), S( -58,   53), S( -19,   52), S(  56,   44), S(  36,   43), S(   1,   14),
            S( -42,   -5), S( -54,   29), S( -68,   46), S(-113,   59), S(-101,   58), S( -64,   52), S( -63,   44), S( -86,   19),
            S( -34,  -17), S( -46,   14), S( -75,   37), S(-102,   52), S(-100,   51), S( -64,   38), S( -67,   27), S( -91,   10),
            S(   8,  -28), S(  23,   -4), S( -32,   17), S( -45,   29), S( -39,   28), S( -37,   20), S(   9,    0), S(  -8,  -12),
            S(  96,  -49), S(  55,  -22), S(  41,   -9), S(   7,    2), S(   5,    5), S(  23,   -4), S(  71,  -23), S(  81,  -41),
            S(  91,  -83), S( 114,  -64), S(  88,  -45), S( -11,  -27), S(  53,  -52), S(  14,  -28), S(  95,  -55), S(  97,  -83),
        }
    },
    S(20, 64)
};

template<typename T>
void addEvalParam(EvalParams& params, const T& t)
{
    params.push_back({static_cast<double>(t[0]), static_cast<double>(t[1])});
}

template<typename T>
void addEvalParamArray(EvalParams& params, const T& t)
{
    for (auto param : t)
        addEvalParam(params, param);
}

template<typename T>
void addEvalParamArray2D(EvalParams& params, const T& t)
{
    for (auto& array : t)
        addEvalParamArray(params, array);
}

EvalParams EvalFn::getInitialParams()
{
    EvalParams params;
    addEvalParamArray2D(params, DEFAULT_PARAMS.psqt);
    for (int i = 0; i < 6; i++)
        for (int j = (i == 0 ? 8 : 0); j < (i == 0 ? 56 : 64); j++)
        {
            params[i * 64 + j].mg += DEFAULT_PARAMS.material[i][0];
            params[i * 64 + j].eg += DEFAULT_PARAMS.material[i][1];
        }
    addEvalParam(params, DEFAULT_PARAMS.bishopPair);
    return params;
}

struct PrintState
{
    const EvalParams& params;
    uint32_t index;
    std::ostringstream ss;
};

template<int ALIGN_SIZE>
void printSingle(PrintState& state)
{
    const EvalParam& param = state.params[state.index++];
    if constexpr (ALIGN_SIZE == 0)
        state.ss << "S(" << param.mg << ", " << param.eg << ')';
    else
        state.ss << "S(" << std::setw(ALIGN_SIZE) << param.mg << ", " << std::setw(ALIGN_SIZE) << param.eg << ')';
}

template<int ALIGN_SIZE>
void printPSQTs(PrintState& state)
{
    state.ss << "PSQT: {\n";
    for (int pce = 0; pce < 6; pce++)
    {
        state.ss << "\t{\n";
        for (int y = 0; y < 8; y++)
        {
            state.ss << "\t\t";
            for (int x = 0; x < 8; x++)
            {
                printSingle<ALIGN_SIZE>(state);
                state.ss << ", ";
            }
            state.ss << "\n";
        }
        state.ss << "\t},\n";
    }
    state.ss << "}\n";
}

void printMaterial(PrintState& state)
{
    state.ss << "Material: {";
    for (int j = 0; j < 5; j++)
    {
        printSingle<4>(state);
        state.ss << ", ";
    }
    state.ss << "}\n";
}

void EvalFn::printEvalParams(const EvalParams& params, std::ostream& os)
{
    PrintState state{params, 0};
    printPSQTs<0>(state);
    state.ss << "bishop pair: ";
    printSingle<0>(state);
    os << state.ss.str() << std::endl;
}

EvalParams extractMaterial(const EvalParams& params)
{
    EvalParams rebalanced = params;
    int material[2][6];
    for (int pce = 0; pce < 6; pce++)
    {
        int begin = (pce == 0 ? 24 : 0), end = (pce == 0 ? 56 : 64);
        int avgMG = 0, avgEG = 0;
        for (int sq = begin; sq < end; sq++)
        {
            avgMG += static_cast<int>(params[pce * 64 + sq].mg);
            avgEG += static_cast<int>(params[pce * 64 + sq].eg);
        }

        avgMG /= (end - begin);
        avgEG /= (end - begin);

        material[0][pce] = avgMG;
        material[1][pce] = avgEG;

        if (pce == 0)
            begin = 8;
        for (int sq = begin; sq < end; sq++)
        {
            rebalanced[pce * 64 + sq].mg -= static_cast<double>(avgMG);
            rebalanced[pce * 64 + sq].eg -= static_cast<double>(avgEG);
        }
    }

    EvalParams extracted;
    for (int i = 0; i < 5; i++)
    {
        extracted.push_back(EvalParam{static_cast<double>(material[0][i]), static_cast<double>(material[1][i])});
    }
    extracted.insert(extracted.end(), rebalanced.begin(), rebalanced.end());
    return extracted;
}

void EvalFn::printEvalParamsExtracted(const EvalParams& params, std::ostream& os)
{
    PrintState state{extractMaterial(params), 0};
    printMaterial(state);
    printPSQTs<4>(state);
    state.ss << "bishop pair: ";
    printSingle<4>(state);
    os << state.ss.str() << std::endl;
}
