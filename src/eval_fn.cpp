#include "eval_fn.h"

#include <sstream>
#include <iomanip>

using namespace chess;

constexpr Bitboard FILE_A = 0x101010101010101;
constexpr Bitboard FILE_H = FILE_A << 7;

constexpr std::array<std::array<Bitboard, 64>, 2> genPassedPawnMasks()
{
    std::array<std::array<Bitboard, 64>, 2> dst = {};

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

constexpr std::array<Bitboard, 64> genIsolatedPawnMasks()
{
    std::array<Bitboard, 64> dst = {};
    for (int i = 0; i < 64; i++)
    {
        Bitboard bb = Bitboard::fromSquare(i);
        bb |= bb << 8;
        bb |= bb << 16;
        bb |= bb << 32;
        bb |= bb >> 8;
        bb |= bb >> 16;
        bb |= bb >> 32;
        dst[i] = ((bb & ~FILE_A) >> 1) | ((bb & ~FILE_H) << 1);
    }
    return dst;
}

constexpr std::array<std::array<Bitboard, 64>, 2> passedPawnMasks = genPassedPawnMasks();
constexpr std::array<Bitboard, 64> isolatedPawnMasks = genIsolatedPawnMasks();

using TraceElem = std::array<int, 2>;

struct Trace
{
    TraceElem psqt[6][64];
    TraceElem knightMobility[9];
    TraceElem bishopMobility[14];
    TraceElem rookMobility[15];
    TraceElem queenMobility[28];
    TraceElem passedPawn[8];
    TraceElem isolatedPawn[8];
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
            if ((isolatedPawnMasks[sq] & pawns[static_cast<int>(color)]).empty())
                trace.isolatedPawn[Square(sq).file()][static_cast<int>(color)]++;
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
    addCoefficientArray(trace.isolatedPawn);
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
    InitialParam isolatedPawn[8];
    InitialParam bishopPair;
    InitialParam openRook[2];
};

#define S(mg, eg) {mg, eg}

constexpr InitialParams DEFAULT_PARAMS = {
    {
        S(64, 99), S(272, 339), S(280, 352), S(366, 628), S(791, 1193), S(0, 0)
    },
    {
        {
            S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
            S(  34,  110), S(  39,   98), S(  22,  101), S(  58,   49), S(  32,   48), S(  14,   57), S( -57,  100), S( -51,  116),
            S(  10,   44), S(  -1,   49), S(  27,   10), S(  32,  -26), S(  39,  -29), S(  70,  -16), S(  40,   25), S(  27,   28),
            S( -12,   29), S( -13,   20), S(  -4,    6), S(  -3,  -11), S(  15,   -9), S(  19,  -11), S(   0,    9), S(  12,    5),
            S( -16,   10), S( -18,   10), S(  -6,   -2), S(   6,   -7), S(   6,   -5), S(   9,   -8), S(  -7,    0), S(   2,   -5),
            S( -18,    6), S( -14,    4), S(  -8,   -1), S(  -3,    0), S(  11,    2), S(   4,   -4), S(  15,   -6), S(   7,   -8),
            S( -12,    8), S( -12,    6), S(  -5,    2), S(  -5,    1), S(   7,   10), S(  27,   -4), S(  25,   -8), S(   4,   -9),
            S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
        },
        {
            S(-101,  -37), S(-106,   -7), S( -58,    3), S( -24,   -6), S(   8,   -3), S( -40,  -25), S( -83,   -4), S( -60,  -56),
            S(  -7,   -6), S(  -3,    3), S(  23,   -3), S(  38,   -4), S(  26,   -9), S(  76,  -24), S(   4,   -2), S(  35,  -22),
            S(   0,   -5), S(  22,   -2), S(  26,   12), S(  41,   13), S(  70,   -1), S(  82,  -12), S(  46,  -11), S(  35,  -15),
            S(   5,    9), S(   7,   12), S(  22,   21), S(  48,   22), S(  33,   21), S(  46,   19), S(  20,   12), S(  42,   -1),
            S(  -5,   13), S(  -2,    6), S(   5,   22), S(  15,   23), S(  18,   27), S(  18,   14), S(  22,    5), S(   9,    9),
            S( -23,   -1), S( -13,    0), S( -10,    5), S(  -7,   19), S(   7,   16), S(  -6,    1), S(   9,   -3), S(  -7,    0),
            S( -24,    0), S( -21,    2), S( -14,   -3), S(   2,   -2), S(   0,   -2), S(  -1,   -6), S(  -4,   -5), S(  -4,   11),
            S( -59,   10), S( -12,   -9), S( -31,   -6), S( -17,   -5), S( -12,    0), S(  -8,  -11), S( -10,   -4), S( -31,    6),
        },
        {
            S( -19,    1), S( -54,    9), S( -44,    4), S( -85,   14), S( -66,    9), S( -55,   -2), S( -33,    2), S( -45,   -6),
            S( -13,  -11), S(  -6,   -5), S( -12,   -2), S( -19,    1), S(   4,  -11), S(  -4,   -5), S(  -9,   -3), S(   2,  -13),
            S(  -2,    7), S(  10,   -3), S(   8,    2), S(  20,   -8), S(  12,   -2), S(  47,    0), S(  28,   -3), S(  26,    3),
            S( -11,    4), S(   4,    7), S(  10,    2), S(  27,   15), S(  20,    7), S(  15,    8), S(   6,    3), S(  -7,    6),
            S(  -2,   -1), S( -10,    6), S(   0,   12), S(  18,   11), S(  16,   10), S(  -3,    6), S(  -6,    6), S(  15,  -11),
            S(   3,    1), S(  10,    5), S(   3,    7), S(   8,   10), S(   8,   13), S(   6,    7), S(   9,   -4), S(  19,   -4),
            S(  24,    8), S(   7,   -8), S(  18,  -10), S(  -3,    0), S(   4,    2), S(  15,   -7), S(  24,   -2), S(  21,   -7),
            S(   9,   -4), S(  26,    2), S(   7,   -7), S(  -6,   -3), S(  -1,   -5), S(  -1,    5), S(  18,  -13), S(  25,  -16),
        },
        {
            S(   4,   14), S(  -7,   20), S(  -9,   26), S( -10,   22), S(  10,   15), S(  32,   11), S(  28,   13), S(  44,    7),
            S( -13,   15), S( -13,   24), S(   1,   26), S(  16,   16), S(   6,   17), S(  34,    7), S(  34,    4), S(  59,   -6),
            S( -22,   13), S(   4,   12), S(  -1,   13), S(   2,   10), S(  29,   -1), S(  44,   -9), S(  86,  -15), S(  61,  -18),
            S( -24,   15), S( -11,   10), S( -13,   17), S(  -8,   12), S(  -1,   -1), S(   8,   -5), S(  26,   -6), S(  21,  -10),
            S( -31,    7), S( -34,    9), S( -27,    7), S( -21,    5), S( -18,    3), S( -21,    2), S(   9,   -8), S(  -3,  -10),
            S( -34,    0), S( -31,   -2), S( -28,   -5), S( -24,   -4), S( -15,   -9), S(  -8,  -15), S(  25,  -31), S(   4,  -27),
            S( -31,   -8), S( -28,   -5), S( -19,   -6), S( -16,   -8), S( -10,  -15), S(  -2,  -20), S(  12,  -26), S( -16,  -19),
            S( -14,   -3), S( -13,   -6), S( -10,   -1), S(  -3,   -8), S(   4,  -15), S(   2,   -9), S(  11,  -18), S(  -8,  -17),
        },
        {
            S( -12,  -12), S( -34,   16), S( -17,   36), S(  13,   25), S(  13,   23), S(  27,   15), S(  65,  -37), S(  21,   -5),
            S(  -4,  -21), S( -35,   13), S( -32,   45), S( -42,   67), S( -35,   82), S(   2,   39), S( -11,   29), S(  51,   10),
            S(   0,  -15), S( -10,   -3), S( -15,   31), S(  -6,   40), S(   6,   51), S(  49,   31), S(  58,   -1), S(  64,    1),
            S( -15,   -4), S( -13,    7), S( -17,   18), S( -16,   37), S( -17,   54), S(  -1,   42), S(   9,   40), S(  17,   25),
            S(  -8,  -14), S( -18,    9), S( -18,   13), S( -13,   29), S( -14,   30), S( -12,   26), S(   1,   17), S(  10,   15),
            S(  -9,  -28), S(  -7,  -13), S( -14,    4), S( -13,    2), S( -10,    7), S(  -4,    2), S(   9,  -13), S(  11,  -23),
            S(   0,  -35), S(  -7,  -33), S(   1,  -34), S(   3,  -29), S(   1,  -24), S(   9,  -48), S(  17,  -73), S(  35,  -91),
            S(  -7,  -37), S(  -4,  -37), S(   0,  -39), S(   5,  -28), S(   3,  -41), S(  -9,  -39), S(  12,  -59), S(  13,  -66),
        },
        {
            S(  69, -106), S(  61,  -56), S(  76,  -42), S( -52,    4), S( -10,  -10), S(  47,  -13), S(  99,  -22), S( 205, -128),
            S( -60,  -11), S(  -7,   16), S( -43,   28), S(  54,   13), S(   9,   30), S(   8,   43), S(  49,   30), S(  26,    0),
            S( -70,    1), S(  36,   21), S( -27,   39), S( -47,   50), S( -10,   51), S(  65,   42), S(  44,   40), S(   4,   10),
            S( -38,   -9), S( -47,   25), S( -67,   45), S(-111,   58), S(-102,   58), S( -64,   53), S( -57,   40), S( -83,   15),
            S( -41,  -18), S( -45,   13), S( -76,   37), S(-109,   53), S(-106,   52), S( -67,   38), S( -69,   25), S(-100,   10),
            S(   5,  -29), S(  21,   -4), S( -38,   19), S( -54,   31), S( -46,   31), S( -44,   22), S(   0,    3), S( -18,   -9),
            S(  91,  -46), S(  48,  -19), S(  33,   -7), S(  -1,    3), S(  -6,    8), S(  14,   -2), S(  59,  -19), S(  67,  -37),
            S(  80,  -82), S( 105,  -63), S(  80,  -44), S( -10,  -26), S(  47,  -44), S(  14,  -27), S(  84,  -54), S(  84,  -84),
        },
    },
    {S(-135, -104), S( -38,  -48), S( -16,  -20), S(  -7,    2), S(   3,   10), S(   4,   21), S(  15,   23), S(  24,   25), S(  35,   21)},
    {S( -31, -162), S( -55,  -78), S( -22,  -33), S( -11,  -10), S(   1,   -1), S(   9,    6), S(  16,   15), S(  22,   20), S(  25,   25), S(  29,   25), S(  32,   27), S(  45,   20), S(  53,   20), S(  62,   10)},
    {S( -62, -121), S( -45,  -68), S( -19,  -31), S( -11,  -16), S(  -5,   -7), S(  -1,   -0), S(   2,    5), S(   4,   11), S(   8,   11), S(  14,   14), S(  18,   17), S(  21,   21), S(  26,   23), S(  32,   22), S(  33,   22)},
    {S(   0,    0), S(   0,    0), S(-174,  -51), S( -35, -226), S( -48,  -95), S( -19,  -74), S( -11,  -59), S(  -6,  -41), S(  -2,  -24), S(   1,    2), S(   5,    7), S(  10,   13), S(  14,   20), S(  19,   20), S(  21,   24), S(  23,   32), S(  24,   34), S(  24,   43), S(  25,   48), S(  26,   50), S(  33,   54), S(  40,   45), S(  54,   45), S(  69,   34), S(  81,   34), S( 191,  -19), S( 114,   13), S(  73,   24)},
    {S(   0,    0), S(  -5,   12), S(  -9,   17), S(  -8,   40), S(  12,   62), S(   3,  125), S(  23,  102), S(   0,    0)},
    {S( -10,  -11), S(  -7,  -15), S( -18,  -17), S( -18,  -22), S( -21,  -24), S( -15,  -14), S( -10,  -14), S( -21,  -10)},
    S(21, 64),
    {S(32, 8), S(13, 11)}
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
    addEvalParamArray(params, DEFAULT_PARAMS.isolatedPawn);
    addEvalParam(params, DEFAULT_PARAMS.bishopPair);
    addEvalParamArray(params, DEFAULT_PARAMS.openRook);
    return params;
}

EvalParams EvalFn::getMaterialParams()
{
    EvalParams params = getInitialParams();
    std::fill(params.begin(), params.end(), EvalParam{0, 0});

    for (int i = 0; i < 6; i++)
        for (int j = (i == 0 ? 8 : 0); j < (i == 0 ? 56 : 64); j++)
        {
            params[i * 64 + j].mg += DEFAULT_PARAMS.material[i][0];
            params[i * 64 + j].eg += DEFAULT_PARAMS.material[i][1];
        }
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

    state.ss << "isolated pawn: ";
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

    state.ss << "isolated pawn: ";
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
