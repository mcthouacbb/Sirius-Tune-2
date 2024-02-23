#include "eval_fn.h"

#include <sstream>
#include <iomanip>

using TraceElem = std::array<int, 2>;

struct Trace
{
    TraceElem psqt[6][64];
    TraceElem bishopPair;
    TraceElem openRook[2];
};

Trace getTrace(const chess::Board& board)
{
    using namespace chess;
    Trace trace = {};

    std::array<Bitboard, 2> pawns = {
        board.pieces(PieceType::PAWN, Color::WHITE),
        board.pieces(PieceType::PAWN, Color::BLACK)
    };

    for (int sq = 0; sq < 64; sq++)
    {
        Piece pce = board.at(static_cast<Square>(sq));
        if (pce == Piece::NONE)
            continue;

        PieceType type = pce.type();
        Color color = pce.color();

        // flip if white
        int square = sq ^ (color == Color::WHITE ? 0b111000 : 0);

        trace.psqt[static_cast<int>(type)][square][static_cast<int>(color)]++;

        if (type == PieceType::ROOK)
        {
            Bitboard fileBB = Bitboard(Square(sq).file());
            if ((fileBB & pawns[static_cast<int>(color)]).empty())
            {
                if ((fileBB & pawns[static_cast<int>(~color)]).empty())
                    // fully open
                    trace.openRook[0][static_cast<int>(color)]++;
                else
                    // semi open
                    trace.openRook[1][static_cast<int>(color)]++;
            }
        }
    }

    if (board.pieces(PieceType::BISHOP, Color::WHITE).count() >= 2)
        trace.bishopPair[static_cast<int>(Color::WHITE)] = 1;
    if (board.pieces(PieceType::BISHOP, Color::BLACK).count() >= 2)
        trace.bishopPair[static_cast<int>(Color::BLACK)] = 1;

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
    addCoefficientArray(trace.openRook);
    return {pos, m_Coefficients.size()};
}

using InitialParam = std::array<int, 2>;

struct InitialParams
{
    InitialParam material[6];
    InitialParam psqt[6][64];
    InitialParam bishopPair;
    InitialParam openRook[2];
};

#define S(mg, eg) {mg, eg}

constexpr InitialParams DEFAULT_PARAMS = {
    {
        S(67, 120), S(269, 339), S(283, 356), S(358, 626), S(780, 1198), S(0, 0)
    },
    {
        {
            S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
            S(  78,  166), S(  94,  158), S(  75,  158), S( 111,  110), S(  89,  105), S(  71,  116), S(  -4,  162), S( -21,  176),
            S(   1,  105), S(   7,  114), S(  42,   80), S(  49,   60), S(  51,   51), S(  75,   34), S(  47,   82), S(  12,   79),
            S( -15,   35), S(   3,   26), S(   7,    6), S(   9,   -2), S(  30,  -12), S(  26,  -10), S(  24,   10), S(   5,   10),
            S( -25,   11), S(  -4,    9), S(  -7,   -8), S(  10,  -11), S(  11,  -14), S(   8,  -14), S(  12,   -1), S(  -7,   -8),
            S( -27,    5), S(  -9,    8), S( -11,  -10), S(  -9,    3), S(   6,   -5), S(   0,   -9), S(  26,   -3), S(  -1,  -12),
            S( -27,    9), S(  -9,   12), S( -15,   -2), S( -23,    3), S(  -4,    7), S(  17,   -5), S(  34,   -3), S(  -9,  -11),
            S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
        },
        {
            S(-137,  -73), S(-112,  -15), S( -49,   -2), S( -16,  -10), S(  16,   -7), S( -40,  -30), S( -90,  -10), S( -87,  -95),
            S( -16,  -23), S(   4,   -2), S(  29,    6), S(  44,    6), S(  28,   -2), S(  86,  -16), S(   4,   -7), S(  22,  -38),
            S(   0,   -7), S(  34,    9), S(  52,   25), S(  64,   25), S(  99,   10), S( 100,    4), S(  57,   -2), S(  29,  -18),
            S(  -1,    5), S(  13,   25), S(  36,   36), S(  57,   38), S(  40,   39), S(  64,   32), S(  23,   24), S(  33,   -4),
            S( -14,    7), S(   1,   15), S(  17,   38), S(  19,   39), S(  27,   42), S(  22,   31), S(  20,   17), S(  -2,   -2),
            S( -31,   -7), S(  -9,   10), S(   4,   20), S(   8,   32), S(  19,   31), S(   8,   16), S(  13,    6), S( -14,   -7),
            S( -44,  -17), S( -32,   -2), S( -16,    8), S(  -4,   12), S(  -3,   11), S(   0,    5), S( -13,  -11), S( -16,   -7),
            S( -83,  -25), S( -34,  -35), S( -45,   -8), S( -32,   -4), S( -27,   -2), S( -12,  -15), S( -30,  -28), S( -55,  -37),
        },
        {
            S( -23,   -8), S( -42,    1), S( -34,   -1), S( -76,   12), S( -65,    7), S( -47,   -3), S( -22,   -7), S( -50,  -13),
            S( -12,  -23), S(  15,   -4), S(   6,    0), S(  -9,    4), S(  18,   -6), S(  15,   -5), S(  10,    1), S(   0,  -24),
            S(  -1,    5), S(  23,    1), S(  24,   12), S(  43,    0), S(  33,    6), S(  62,    7), S(  41,    0), S(  27,   -1),
            S(  -9,    1), S(   6,   17), S(  26,   12), S(  37,   27), S(  35,   17), S(  31,   17), S(   7,   13), S(  -6,    1),
            S( -14,   -3), S(  -2,   15), S(   5,   23), S(  26,   19), S(  22,   20), S(   7,   17), S(   0,   12), S(  -5,  -13),
            S(  -5,   -3), S(   4,    8), S(   4,   14), S(   6,   15), S(   8,   19), S(   3,   16), S(   6,   -1), S(   9,  -13),
            S(  -1,   -9), S(   1,   -9), S(  12,  -10), S( -10,    4), S(  -2,    6), S(  12,   -5), S(  17,   -2), S(   3,  -28),
            S( -21,  -26), S(   0,   -8), S( -17,  -27), S( -25,   -7), S( -21,   -9), S( -22,   -9), S(   2,  -22), S( -10,  -40),
        },
        {
            S(  12,   14), S(   4,   19), S(   7,   26), S(  12,   20), S(  30,   14), S(  51,    8), S(  43,    8), S(  55,    6),
            S(  -6,   15), S(  -4,   23), S(  12,   25), S(  27,   14), S(  15,   16), S(  44,    8), S(  39,    4), S(  64,   -8),
            S( -21,   13), S(   4,   13), S(   0,   13), S(  -1,   10), S(  28,   -1), S(  40,   -6), S(  79,  -12), S(  55,  -17),
            S( -25,   12), S( -11,    8), S( -15,   17), S( -10,   11), S(  -4,   -2), S(   9,   -6), S(  20,   -8), S(  21,  -13),
            S( -33,    3), S( -36,    7), S( -31,    7), S( -22,    5), S( -20,    1), S( -26,    2), S(   2,  -10), S(  -4,  -16),
            S( -36,   -2), S( -35,   -2), S( -31,   -4), S( -30,   -2), S( -21,   -6), S( -16,  -12), S(  21,  -30), S(   2,  -30),
            S( -35,   -8), S( -31,   -4), S( -25,   -2), S( -26,   -3), S( -19,  -10), S(  -8,  -14), S(   6,  -23), S( -19,  -19),
            S( -15,   -4), S( -19,   -2), S( -20,    6), S( -12,   -1), S(  -6,   -8), S(  -4,   -5), S(   3,  -13), S( -12,  -16),
        },
        {
            S( -36,    1), S( -31,   17), S(  -2,   35), S(  31,   21), S(  32,   17), S(  35,   11), S(  61,  -37), S(   5,   -4),
            S(  -3,  -33), S( -21,    8), S( -17,   44), S( -24,   62), S( -20,   80), S(  15,   38), S(   0,   21), S(  41,   -3),
            S(   0,  -25), S(  -4,   -4), S(  -3,   36), S(  10,   41), S(  18,   52), S(  57,   32), S(  59,   -6), S(  58,  -19),
            S( -15,  -14), S( -10,    9), S(  -7,   25), S(  -7,   47), S(  -6,   62), S(   9,   45), S(   9,   30), S(  15,    8),
            S( -12,  -17), S( -13,   12), S( -14,   21), S(  -6,   40), S(  -7,   37), S(  -7,   29), S(   4,   11), S(   6,   -1),
            S( -14,  -26), S(  -7,  -10), S( -11,   12), S( -12,    7), S(  -9,   12), S(  -2,    6), S(  10,  -15), S(   4,  -30),
            S( -15,  -31), S(  -9,  -28), S(   0,  -32), S(   0,  -22), S(  -2,  -18), S(   8,  -44), S(  14,  -71), S(  22,  -98),
            S( -18,  -34), S( -26,  -29), S( -19,  -25), S(  -6,  -31), S( -14,  -28), S( -25,  -31), S(  -2,  -62), S( -12,  -61),
        },
        {
            S(  64, -104), S(  37,  -53), S(  76,  -45), S( -69,    5), S( -13,  -15), S(  38,  -12), S(  92,  -21), S( 199, -126),
            S( -55,  -11), S( -12,   17), S( -57,   30), S(  50,   11), S(  -2,   32), S(   0,   44), S(  40,   34), S(  20,    3),
            S( -74,    4), S(  28,   22), S( -37,   41), S( -59,   52), S( -20,   51), S(  52,   44), S(  34,   43), S(  -1,   14),
            S( -41,   -6), S( -53,   28), S( -69,   46), S(-115,   59), S(-102,   58), S( -64,   52), S( -63,   44), S( -87,   19),
            S( -30,  -18), S( -43,   13), S( -74,   37), S(-104,   51), S(-100,   51), S( -64,   38), S( -66,   26), S( -91,   10),
            S(  11,  -29), S(  28,   -5), S( -30,   16), S( -46,   29), S( -39,   28), S( -36,   20), S(   9,    0), S(  -8,  -12),
            S( 100,  -49), S(  58,  -22), S(  43,  -10), S(   7,    1), S(   4,    5), S(  24,   -5), S(  70,  -23), S(  79,  -40),
            S(  92,  -84), S( 116,  -65), S(  89,  -47), S( -10,  -27), S(  51,  -50), S(  15,  -29), S(  94,  -55), S(  95,  -83),
        }
    },
    S(20, 64),
    {S(40, 13), S(16, 9)}
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
    addEvalParamArray(params, DEFAULT_PARAMS.openRook);
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

template<int ALIGN_SIZE>
void printArray(PrintState& state, int len)
{
    state.ss << '{';
    for (int i = 0; i < len; i++)
    {
        printSingle<ALIGN_SIZE>(state);
        if (i != len - 1)
            state.ss << ", ";
    }
    state.ss << '}';
}

void EvalFn::printEvalParams(const EvalParams& params, std::ostream& os)
{
    PrintState state{params, 0};
    printPSQTs<0>(state);
    state.ss << "bishop pair: ";
    printSingle<0>(state);
    state.ss << '\n';

    state.ss << "open rook: ";
    printArray<0>(state, 2);
    state.ss << '\n';
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
    state.ss << '\n';

    state.ss << "open rook: ";
    printArray<4>(state, 2);
    state.ss << '\n';
    os << state.ss.str() << std::endl;
}
