#include "eval_fn.h"

#include <sstream>
#include <iomanip>

using namespace chess;

constexpr std::array<std::array<Bitboard, 64>, 2> genPassedPawnMasks()
{
    constexpr Bitboard FILE_A = 0x101010101010101;
    constexpr Bitboard FILE_H = FILE_A << 7;
    std::array<std::array<Bitboard, 64>, 2> dst{};

    for (int i = 0; i < 64; i++)
    {
        Bitboard bb = Bitboard::fromSquare(i) << 8;
        bb |= bb << 8;
        bb |= bb << 16;
        bb |= bb << 32;

        dst[0][i] = bb | ((bb & ~FILE_A) >> 1) | ((bb & ~FILE_H) << 1);

        bb = Bitboard::fromSquare(i) >> 8;
        bb |= bb >> 8;
        bb |= bb >> 16;
        bb |= bb >> 32;

        dst[1][i] = bb | ((bb & ~FILE_A) >> 1) | ((bb & ~FILE_H) << 1);
    }

    return dst;
}

constexpr std::array<std::array<Bitboard, 64>, 2> passedPawnMasks = genPassedPawnMasks();

using TraceElem = std::array<int, 2>;

struct Trace
{
    TraceElem psqt[6][64];
    TraceElem knightMobility[9];
    TraceElem bishopMobility[14];
    TraceElem rookMobility[15];
    TraceElem queenMobility[28];
    TraceElem passedPawn[8];
    TraceElem bishopPair;
    TraceElem openRook[2];
};

Trace getTrace(const chess::Board& board)
{
    Trace trace = {};

    std::array<Bitboard, 2> pawns = {
        board.pieces(PieceType::PAWN, Color::WHITE),
        board.pieces(PieceType::PAWN, Color::BLACK)
    };

    std::array<Bitboard, 2> mobilityArea = {
        ~(attacks::pawnLeftAttacks<Color::BLACK>(pawns[1]) | attacks::pawnRightAttacks<Color::BLACK>(pawns[1])),
        ~(attacks::pawnLeftAttacks<Color::WHITE>(pawns[0]) | attacks::pawnRightAttacks<Color::WHITE>(pawns[0]))
    };

    for (int sq = 0; sq < 64; sq++)
    {
        Piece pce = board.at(static_cast<Square>(sq));
        if (pce == Piece::NONE)
            continue;

        PieceType type = pce.type();
        Color color = pce.color();

        // flip if white
        int psqtSquare = sq ^ (color == Color::WHITE ? 0b111000 : 0);

        trace.psqt[static_cast<int>(type)][psqtSquare][static_cast<int>(color)]++;

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

        if (type == PieceType::PAWN)
        {
            if ((passedPawnMasks[static_cast<int>(color)][sq] & pawns[static_cast<int>(~color)]).empty())
                trace.passedPawn[Square(sq).relative_square(color).rank()][static_cast<int>(color)]++;
        }

        switch (type.internal())
        {
            case PieceType::KNIGHT:
            {
                Bitboard attacksBB = attacks::knight(sq);
                int mobility = (attacksBB & mobilityArea[static_cast<int>(color)]).count();
                trace.knightMobility[mobility][static_cast<int>(color)]++;
                break;
            }
            case PieceType::BISHOP:
            {
                Bitboard attacksBB = attacks::bishop(sq, board.occ());
                int mobility = (attacksBB & mobilityArea[static_cast<int>(color)]).count();
                trace.bishopMobility[mobility][static_cast<int>(color)]++;
                break;
            }
            case PieceType::ROOK:
            {
                Bitboard attacksBB = attacks::rook(sq, board.occ());
                int mobility = (attacksBB & mobilityArea[static_cast<int>(color)]).count();
                trace.rookMobility[mobility][static_cast<int>(color)]++;
                break;
            }
            case PieceType::QUEEN:
            {
                Bitboard attacksBB = attacks::queen(sq, board.occ());
                int mobility = (attacksBB & mobilityArea[static_cast<int>(color)]).count();
                trace.queenMobility[mobility][static_cast<int>(color)]++;
                break;
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
    addCoefficientArray(trace.knightMobility);
    addCoefficientArray(trace.bishopMobility);
    addCoefficientArray(trace.rookMobility);
    addCoefficientArray(trace.queenMobility);
    addCoefficientArray(trace.passedPawn);
    addCoefficient(trace.bishopPair);
    addCoefficientArray(trace.openRook);
    return {pos, m_Coefficients.size()};
}

using InitialParam = std::array<int, 2>;

struct InitialParams
{
    InitialParam material[6];
    InitialParam psqt[6][64];
    InitialParam knightMobility[9];
    InitialParam bishopMobility[14];
    InitialParam rookMobility[15];
    InitialParam queenMobility[28];
    InitialParam passedPawn[8];
    InitialParam bishopPair;
    InitialParam openRook[2];
};

#define S(mg, eg) {mg, eg}

constexpr InitialParams DEFAULT_PARAMS = {
    {
        S(62, 120), S(273, 338), S(282, 350), S(361, 621), S(789, 1185), S(0, 0)
    },
    {
        {
            S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
            S(  55,  169), S(  73,  161), S(  47,  163), S(  78,  113), S(  59,  109), S(  45,  122), S( -21,  165), S( -36,  178),
            S(  -5,  106), S(  -6,  117), S(  20,   85), S(  28,   64), S(  31,   54), S(  54,   39), S(  37,   85), S(   5,   81),
            S( -19,   36), S(  -7,   27), S(  -6,    8), S(  -5,    1), S(  13,   -8), S(  14,   -8), S(  10,   12), S(   1,   11),
            S( -23,   10), S( -11,   10), S(  -8,   -9), S(   3,  -11), S(   6,  -13), S(   6,  -14), S(   5,    0), S(  -7,   -8),
            S( -24,    5), S(  -7,    6), S( -10,   -9), S(  -4,    1), S(  12,   -7), S(   0,   -9), S(  26,   -3), S(  -1,  -11),
            S( -18,    7), S(  -3,   10), S(  -6,   -5), S(  -4,    3), S(   8,    5), S(  26,   -6), S(  38,   -4), S(  -3,  -13),
            S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
        },
        {
            S( -96,  -36), S(-106,   -4), S( -56,    2), S( -20,   -6), S(  10,   -3), S( -36,  -27), S( -80,    1), S( -54,  -56),
            S(  -4,   -9), S(   2,    3), S(  24,   -2), S(  36,   -2), S(  26,   -9), S(  77,  -24), S(  10,   -3), S(  37,  -24),
            S(  -1,   -4), S(  20,    0), S(  24,   14), S(  38,   14), S(  68,    1), S(  77,   -6), S(  44,  -11), S(  34,  -14),
            S(   4,    8), S(   6,   13), S(  19,   22), S(  45,   23), S(  30,   23), S(  44,   20), S(  19,   13), S(  41,    0),
            S(  -5,   13), S(  -3,    5), S(   2,   24), S(  13,   23), S(  16,   27), S(  16,   17), S(  20,    7), S(   8,    9),
            S( -23,   -1), S( -15,    0), S( -12,    5), S( -10,   19), S(   4,   18), S(  -9,    2), S(   6,   -2), S(  -7,    0),
            S( -24,   -3), S( -21,    2), S( -16,   -4), S(   0,    0), S(  -2,   -1), S(  -2,   -5), S(  -5,   -5), S(  -5,   11),
            S( -61,    8), S( -11,   -9), S( -32,   -7), S( -19,   -3), S( -14,    1), S( -10,   -9), S( -10,   -3), S( -30,   -3),
        },
        {
            S( -16,    0), S( -52,    7), S( -41,    4), S( -84,   15), S( -67,    9), S( -52,   -2), S( -32,   -3), S( -41,   -6),
            S( -11,  -14), S(  -1,   -6), S( -11,   -3), S( -19,    0), S(   4,  -10), S(  -2,   -8), S(  -2,   -2), S(   4,  -15),
            S(  -5,    8), S(   9,   -2), S(   7,    4), S(  20,   -7), S(  11,   -1), S(  47,   -1), S(  27,   -3), S(  25,    3),
            S( -11,    3), S(   2,    8), S(  10,    3), S(  25,   16), S(  20,    7), S(  15,    9), S(   6,    3), S(  -7,    5),
            S(  -3,   -3), S( -11,    7), S(  -2,   12), S(  17,    9), S(  15,   10), S(  -3,    6), S(  -7,    6), S(  14,  -12),
            S(   4,    1), S(   8,    4), S(   3,    6), S(   6,    8), S(   7,   11), S(   4,    7), S(   7,   -4), S(  20,   -5),
            S(  24,    6), S(   6,   -7), S(  16,  -11), S(  -3,    0), S(   3,    2), S(  15,   -7), S(  23,   -1), S(  21,   -9),
            S(  10,   -4), S(  24,    2), S(   7,   -8), S(  -8,   -3), S(  -2,   -5), S(  -2,    5), S(  20,  -13), S(  25,  -15),
        },
        {
            S(   6,   13), S(  -5,   19), S(  -8,   27), S(  -6,   22), S(  13,   15), S(  37,    9), S(  37,    8), S(  48,    5),
            S( -13,   15), S( -11,   24), S(   2,   27), S(  18,   16), S(   7,   17), S(  35,    8), S(  36,    4), S(  59,   -8),
            S( -24,   14), S(   4,   14), S(  -1,   15), S(   1,   12), S(  27,    1), S(  42,   -5), S(  83,  -12), S(  59,  -17),
            S( -24,   14), S(  -9,   10), S( -14,   18), S(  -9,   13), S(  -1,    0), S(   9,   -4), S(  26,   -7), S(  22,  -11),
            S( -30,    4), S( -34,    8), S( -27,    8), S( -20,    5), S( -18,    3), S( -21,    2), S(   8,   -9), S(  -2,  -13),
            S( -33,    0), S( -30,   -2), S( -27,   -4), S( -24,   -3), S( -15,   -8), S(  -8,  -15), S(  24,  -30), S(   4,  -28),
            S( -31,   -7), S( -27,   -5), S( -18,   -5), S( -16,   -6), S(  -9,  -13), S(  -2,  -18), S(  11,  -24), S( -14,  -19),
            S( -14,   -2), S( -13,   -5), S( -10,    0), S(  -3,   -6), S(   4,  -13), S(   2,   -8), S(   9,  -17), S(  -8,  -16),
        },
        {
            S(  -8,  -15), S( -35,   16), S( -15,   36), S(  12,   24), S(  17,   20), S(  22,   18), S(  63,  -37), S(  25,  -11),
            S(  -3,  -25), S( -30,   10), S( -31,   45), S( -39,   63), S( -34,   81), S(   3,   40), S(  -6,   22), S(  50,    7),
            S(   0,  -16), S( -10,   -3), S( -14,   33), S(  -6,   43), S(   5,   52), S(  50,   31), S(  56,   -1), S(  62,   -2),
            S( -14,   -8), S( -13,    6), S( -18,   20), S( -17,   38), S( -17,   54), S(  -1,   42), S(  10,   36), S(  18,   22),
            S(  -8,  -14), S( -19,   10), S( -19,   15), S( -13,   30), S( -14,   29), S( -13,   26), S(   0,   17), S(  11,   11),
            S( -10,  -24), S(  -8,  -11), S( -15,    4), S( -14,    2), S( -11,    6), S(  -5,    2), S(   7,  -12), S(  11,  -22),
            S(   0,  -34), S(  -8,  -31), S(  -1,  -32), S(   2,  -28), S(   0,  -23), S(   8,  -45), S(  16,  -68), S(  35,  -88),
            S(  -7,  -37), S(  -5,  -35), S(  -1,  -37), S(   5,  -26), S(   2,  -40), S( -10,  -36), S(  12,  -58), S(  13,  -66),
        },
        {
            S(  69, -103), S(  52,  -54), S(  86,  -46), S( -56,    3), S(  -1,  -15), S(  43,  -13), S(  88,  -21), S( 202, -128),
            S( -55,  -11), S(  -2,   16), S( -45,   28), S(  60,   11), S(   8,   31), S(  10,   43), S(  39,   34), S(  18,    3),
            S( -71,    4), S(  33,   22), S( -29,   41), S( -49,   51), S( -12,   51), S(  60,   43), S(  36,   42), S(   1,   13),
            S( -40,   -6), S( -49,   27), S( -66,   46), S(-109,   58), S( -99,   58), S( -63,   52), S( -60,   43), S( -88,   19),
            S( -31,  -19), S( -43,   13), S( -71,   36), S(-102,   50), S(-101,   50), S( -64,   37), S( -67,   26), S( -93,   10),
            S(   5,  -28), S(  23,   -5), S( -34,   16), S( -49,   28), S( -42,   27), S( -40,   19), S(   4,    0), S( -14,  -11),
            S(  90,  -48), S(  49,  -21), S(  36,   -9), S(   2,    2), S(  -2,    5), S(  17,   -4), S(  63,  -22), S(  69,  -39),
            S(  79,  -82), S( 107,  -64), S(  83,  -45), S(  -7,  -27), S(  50,  -45), S(  16,  -29), S(  86,  -55), S(  84,  -84),
        }
    },
    {S(-145,  -94), S( -42,  -55), S( -19,  -29), S( -10,   -8), S(   1,   -0), S(   3,   10), S(  14,   13), S(  24,   13), S(  35,    9)},
    {S( -47, -138), S( -63,  -83), S( -27,  -44), S( -15,  -20), S(  -3,  -11), S(   6,   -4), S(  13,    5), S(  20,    9), S(  23,   15), S(  27,   14), S(  30,   16), S(  42,    9), S(  53,    7), S(  59,   -2)},
    {S( -56, -154), S( -41,  -81), S( -16,  -40), S(  -7,  -26), S(  -1,  -18), S(   2,  -11), S(   5,   -5), S(   8,    0), S(  12,    1), S(  18,    4), S(  22,    7), S(  25,   10), S(  29,   11), S(  33,    9), S(  30,   10)},
    {S(   0,    0), S(   0,    0), S(-230,  -66), S( -32, -275), S( -50, -116), S( -21,  -93), S( -12,  -81), S(  -7,  -67), S(  -2,  -50), S(   1,  -25), S(   5,  -20), S(  10,  -14), S(  15,   -7), S(  20,   -7), S(  22,   -3), S(  24,    4), S(  25,    8), S(  25,   16), S(  25,   21), S(  26,   22), S(  33,   24), S(  39,   17), S(  53,   14), S(  66,    4), S(  79,    2), S( 186,  -48), S( 115,  -22), S(  82,  -12)},
    {S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)},
    S(20, 64),
    {S(31, 8), S(12, 9)}
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
    addEvalParamArray(params, DEFAULT_PARAMS.knightMobility);
    addEvalParamArray(params, DEFAULT_PARAMS.bishopMobility);
    addEvalParamArray(params, DEFAULT_PARAMS.rookMobility);
    addEvalParamArray(params, DEFAULT_PARAMS.queenMobility);
    addEvalParamArray(params, DEFAULT_PARAMS.passedPawn);
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

template<int ALIGN_SIZE>
void printMobility(PrintState& state)
{
    state.ss << "knight mobility: ";
    printArray<ALIGN_SIZE>(state, 9);
    state.ss << '\n';

    state.ss << "bishop mobility: ";
    printArray<ALIGN_SIZE>(state, 14);
    state.ss << '\n';

    state.ss << "rook mobility: ";
    printArray<ALIGN_SIZE>(state, 15);
    state.ss << '\n';

    state.ss << "queen mobility: ";
    printArray<ALIGN_SIZE>(state, 28);
    state.ss << '\n';
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
    printMobility<0>(state);

    state.ss << "passed pawn: ";
    printArray<0>(state, 8);
    state.ss << '\n';

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
    printMobility<4>(state);

    state.ss << "passed pawn: ";
    printArray<4>(state, 8);
    state.ss << '\n';

    state.ss << "bishop pair: ";
    printSingle<4>(state);
    state.ss << '\n';

    state.ss << "open rook: ";
    printArray<4>(state, 2);
    state.ss << '\n';
    os << state.ss.str() << std::endl;
}
