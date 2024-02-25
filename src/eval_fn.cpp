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
        S(68, 107), S(272, 339), S(281, 351), S(360, 624), S(786, 1190), S(0, 0)
    },
    {
        {
            S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
            S(  48,  187), S(  65,  176), S(  42,  175), S(  73,  123), S(  52,  121), S(  38,  137), S( -26,  178), S( -41,  193),
            S(   4,   40), S(   3,   45), S(  24,    3), S(  29,  -31), S(  35,  -35), S(  65,  -17), S(  46,   23), S(  16,   25),
            S( -16,   26), S(  -7,   21), S(  -7,    0), S(  -6,  -15), S(  12,  -16), S(  14,  -12), S(  11,   10), S(   4,    4),
            S( -20,   10), S( -10,   14), S(  -8,   -6), S(   4,  -11), S(   5,  -11), S(   6,   -8), S(   6,    5), S(  -5,   -6),
            S( -21,    6), S(  -6,    9), S( -11,   -6), S(  -5,   -2), S(  10,   -4), S(  -1,   -5), S(  27,    1), S(   1,   -9),
            S( -15,    8), S(  -3,   12), S(  -7,   -4), S(  -5,   -1), S(   7,    5), S(  25,   -3), S(  39,   -1), S(  -1,  -10),
            S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
        },
        {
            S( -97,  -33), S(-102,   -7), S( -56,    3), S( -20,   -6), S(   8,   -3), S( -37,  -25), S( -76,   -4), S( -55,  -53),
            S(  -6,   -4), S(   1,    3), S(  24,   -2), S(  37,   -4), S(  27,  -10), S(  78,  -24), S(   9,   -2), S(  35,  -20),
            S(   0,   -5), S(  21,   -2), S(  24,   11), S(  39,   12), S(  69,   -1), S(  80,  -11), S(  45,  -11), S(  34,  -14),
            S(   5,    9), S(   7,   12), S(  20,   21), S(  47,   21), S(  31,   20), S(  45,   18), S(  19,   12), S(  41,    0),
            S(  -5,   15), S(  -3,    6), S(   3,   21), S(  14,   22), S(  17,   26), S(  17,   14), S(  21,    6), S(   9,   10),
            S( -23,   -1), S( -14,   -1), S( -11,    4), S(  -9,   18), S(   5,   15), S(  -8,   -1), S(   7,   -3), S(  -7,    0),
            S( -24,    1), S( -21,    4), S( -15,   -5), S(   1,   -2), S(  -1,   -2), S(  -2,   -6), S(  -4,   -4), S(  -4,   13),
            S( -61,   14), S( -11,   -8), S( -32,   -5), S( -18,   -3), S( -13,    0), S(  -9,   -9), S(  -9,   -3), S( -31,    5),
        },
        {
            S( -15,    0), S( -53,    9), S( -41,    4), S( -84,   15), S( -65,    9), S( -52,   -2), S( -31,    0), S( -41,   -6),
            S( -12,  -10), S(  -2,   -6), S( -11,   -3), S( -19,   -1), S(   4,  -10), S(  -2,   -7), S(  -3,   -3), S(   2,  -13),
            S(  -4,    7), S(  10,   -3), S(   8,    1), S(  19,   -9), S(  11,   -4), S(  47,   -1), S(  27,   -3), S(  25,    4),
            S( -10,    4), S(   3,    6), S(  10,    1), S(  26,   14), S(  20,    5), S(  16,    7), S(   7,    3), S(  -7,    6),
            S(  -3,   -1), S( -10,    6), S(  -1,   11), S(  18,    9), S(  15,    8), S(  -2,    5), S(  -7,    6), S(  15,  -11),
            S(   4,    1), S(   9,    5), S(   3,    6), S(   7,    8), S(   8,   11), S(   5,    6), S(   8,   -5), S(  20,   -4),
            S(  24,    8), S(   7,   -7), S(  16,  -11), S(  -2,    0), S(   3,    1), S(  16,   -7), S(  24,   -2), S(  21,   -6),
            S(  10,   -2), S(  24,    3), S(   7,   -7), S(  -7,   -2), S(  -1,   -5), S(  -2,    4), S(  19,  -11), S(  26,  -13),
        },
        {
            S(   6,   13), S(  -6,   19), S(  -9,   27), S(  -8,   22), S(  13,   14), S(  35,   10), S(  34,   10), S(  48,    5),
            S( -13,   16), S( -12,   24), S(   2,   26), S(  17,   16), S(   7,   17), S(  35,    8), S(  35,    4), S(  59,   -6),
            S( -24,   14), S(   4,   12), S(  -2,   14), S(   1,   11), S(  28,   -1), S(  43,   -8), S(  84,  -14), S(  59,  -18),
            S( -25,   15), S(  -9,   10), S( -13,   17), S(  -9,   12), S(  -1,   -1), S(  10,   -6), S(  26,   -6), S(  22,  -11),
            S( -30,    6), S( -33,    8), S( -27,    7), S( -20,    5), S( -19,    2), S( -21,    1), S(   7,   -8), S(  -3,  -11),
            S( -33,    0), S( -30,   -2), S( -27,   -4), S( -24,   -3), S( -15,   -8), S(  -8,  -15), S(  24,  -31), S(   4,  -27),
            S( -31,   -7), S( -28,   -5), S( -18,   -6), S( -16,   -8), S( -10,  -14), S(  -1,  -19), S(  11,  -25), S( -14,  -18),
            S( -14,   -3), S( -13,   -7), S( -10,   -1), S(  -3,   -8), S(   4,  -15), S(   2,   -8), S(  10,  -18), S(  -8,  -16),
        },
        {
            S( -10,  -12), S( -34,   15), S( -15,   36), S(  11,   26), S(  13,   24), S(  25,   16), S(  64,  -37), S(  24,   -8),
            S(  -3,  -21), S( -32,   12), S( -31,   44), S( -40,   63), S( -35,   81), S(   3,   39), S(  -8,   27), S(  49,   11),
            S(   0,  -15), S(  -9,   -3), S( -14,   31), S(  -7,   41), S(   4,   50), S(  50,   31), S(  57,    0), S(  62,    2),
            S( -14,   -5), S( -13,    7), S( -18,   19), S( -17,   36), S( -17,   53), S(  -1,   43), S(   9,   39), S(  17,   25),
            S(  -8,  -12), S( -19,   11), S( -18,   13), S( -13,   28), S( -14,   29), S( -13,   26), S(   0,   18), S(  10,   14),
            S( -10,  -25), S(  -8,  -11), S( -15,    4), S( -14,    3), S( -11,    7), S(  -5,    2), S(   7,  -13), S(  11,  -21),
            S(   0,  -32), S(  -7,  -31), S(  -1,  -33), S(   2,  -30), S(   0,  -24), S(   8,  -46), S(  16,  -71), S(  34,  -88),
            S(  -7,  -35), S(  -5,  -35), S(  -1,  -38), S(   5,  -27), S(   2,  -40), S( -10,  -37), S(  11,  -58), S(  13,  -64),
        },
        {
            S(  68, -106), S(  61,  -57), S(  79,  -43), S( -51,    3), S( -10,  -13), S(  42,  -14), S( 102,  -24), S( 220, -133),
            S( -59,  -13), S(  -6,   15), S( -38,   26), S(  54,   12), S(  10,   28), S(   7,   41), S(  49,   29), S(  28,   -1),
            S( -70,    0), S(  37,   20), S( -23,   38), S( -50,   49), S( -12,   49), S(  64,   40), S(  42,   38), S(   5,    9),
            S( -38,   -9), S( -48,   25), S( -65,   45), S(-110,   57), S(-101,   57), S( -63,   51), S( -57,   40), S( -84,   15),
            S( -38,  -18), S( -45,   13), S( -76,   37), S(-110,   53), S(-104,   52), S( -66,   37), S( -67,   25), S( -97,   10),
            S(   6,  -28), S(  21,   -3), S( -37,   19), S( -52,   31), S( -44,   30), S( -42,   21), S(   2,    2), S( -15,   -9),
            S(  91,  -44), S(  48,  -18), S(  35,   -6), S(   1,    4), S(  -3,    8), S(  16,   -2), S(  62,  -20), S(  69,  -37),
            S(  79,  -81), S( 106,  -62), S(  82,  -42), S(  -8,  -25), S(  49,  -44), S(  15,  -27), S(  85,  -54), S(  84,  -83),
        }
    },

    {S(-140, -116), S( -42,  -54), S( -21,  -26), S( -11,   -3), S(  -0,    6), S(   1,   17), S(  12,   20), S(  22,   22), S(  33,   18)},
    {S( -42, -177), S( -62,  -91), S( -28,  -41), S( -17,  -16), S(  -4,   -6), S(   4,    2), S(  11,   12), S(  17,   16), S(  20,   22), S(  24,   23), S(  27,   25), S(  39,   18), S(  50,   18), S(  54,    9)},
    {S( -39, -153), S( -41,  -76), S( -18,  -32), S( -10,  -18), S(  -4,   -9), S(  -0,   -3), S(   3,    2), S(   5,    8), S(   9,    9), S(  15,   12), S(  19,   15), S(  21,   19), S(  25,   21), S(  29,   20), S(  27,   19)},
    {S(   0,    0), S(   0,    0), S(-351, -115), S( -34, -270), S( -54, -104), S( -25,  -84), S( -16,  -70), S( -11,  -54), S(  -7,  -37), S(  -4,  -11), S(   1,   -5), S(   5,    1), S(   9,    9), S(  14,    9), S(  17,   13), S(  19,   21), S(  19,   24), S(  19,   33), S(  19,   38), S(  20,   41), S(  27,   44), S(  34,   35), S(  48,   34), S(  62,   23), S(  72,   26), S( 183,  -30), S( 122,   -5), S( 113,  -13)},

    {S(   0,    0), S( -14,   -1), S( -17,    3), S( -15,   26), S(   7,   50), S(  -1,  113), S(  -3,    7), S(   0,    0)},
    S(21, 63),
    {S(32, 8), S(11, 11)}
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
