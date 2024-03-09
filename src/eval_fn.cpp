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
    TraceElem threats[6][5];
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

    std::array<Bitboard, 2> pawnAttacks = {
        attacks::pawnLeftAttacks<Color::BLACK>(pawns[0]) | attacks::pawnRightAttacks<Color::BLACK>(pawns[0]),
        attacks::pawnLeftAttacks<Color::WHITE>(pawns[1]) | attacks::pawnRightAttacks<Color::WHITE>(pawns[1])
    };

    std::array<Bitboard, 2> mobilityArea = {
        ~pawnAttacks[1],
        ~pawnAttacks[0]
    };

    std::array<Bitboard, 2> pawnThreats = {
        pawnAttacks[0] & board.them(Color::WHITE),
        pawnAttacks[1] & board.them(Color::BLACK)
    };

    for (int i = 0; i < 2; i++)
    {
        Bitboard threats = pawnThreats[i];
        while (threats)
        {
            uint32_t threatened = threats.pop();
            trace.threats[static_cast<int>(PieceType::PAWN)][static_cast<int>(board.at<PieceType>(threatened))][i]++;
        }
    }

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
                Bitboard threats = attacksBB & board.them(static_cast<Color>(color));
                while (threats)
                {
                    uint32_t threatened = threats.pop();
                    trace.threats[static_cast<int>(PieceType::KNIGHT)][static_cast<int>(board.at<PieceType>(threatened))][static_cast<int>(color)]++;
                }
                break;
            }
            case PieceType::BISHOP:
            {
                Bitboard attacksBB = attacks::bishop(sq, board.occ());
                int mobility = (attacksBB & mobilityArea[static_cast<int>(color)]).count();
                trace.bishopMobility[mobility][static_cast<int>(color)]++;
                Bitboard threats = attacksBB & board.them(static_cast<Color>(color));
                while (threats)
                {
                    uint32_t threatened = threats.pop();
                    trace.threats[static_cast<int>(PieceType::BISHOP)][static_cast<int>(board.at<PieceType>(threatened))][static_cast<int>(color)]++;
                }
                break;
            }
            case PieceType::ROOK:
            {
                Bitboard attacksBB = attacks::rook(sq, board.occ());
                int mobility = (attacksBB & mobilityArea[static_cast<int>(color)]).count();
                trace.rookMobility[mobility][static_cast<int>(color)]++;
                Bitboard threats = attacksBB & board.them(static_cast<Color>(color));
                while (threats)
                {
                    uint32_t threatened = threats.pop();
                    trace.threats[static_cast<int>(PieceType::ROOK)][static_cast<int>(board.at<PieceType>(threatened))][static_cast<int>(color)]++;
                }
                break;
            }
            case PieceType::QUEEN:
            {
                Bitboard attacksBB = attacks::queen(sq, board.occ());
                int mobility = (attacksBB & mobilityArea[static_cast<int>(color)]).count();
                trace.queenMobility[mobility][static_cast<int>(color)]++;
                Bitboard threats = attacksBB & board.them(static_cast<Color>(color));
                while (threats)
                {
                    uint32_t threatened = threats.pop();
                    trace.threats[static_cast<int>(PieceType::QUEEN)][static_cast<int>(board.at<PieceType>(threatened))][static_cast<int>(color)]++;
                }
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
    addCoefficientArray2D(trace.threats);
    addCoefficientArray(trace.passedPawn);
    addCoefficientArray(trace.isolatedPawn);
    addCoefficient(trace.bishopPair);
    addCoefficientArray(trace.openRook);
    return {pos, m_Coefficients.size()};
}

using InitialParam = std::array<int, 2>;

#define S(mg, eg) {mg, eg}

constexpr InitialParam MATERIAL[6] = {S(  63,  101), S( 272,  340), S( 279,  352), S( 371,  627), S( 797, 1190), S(0, 0)};

constexpr InitialParam PSQT[6][64] = {
	{
		S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
		S(  34,  105), S(  28,   96), S(  18,   97), S(  54,   44), S(  25,   45), S(  15,   51), S( -70,   98), S( -49,  110),
		S(  10,   43), S(  -3,   49), S(  23,   10), S(  30,  -26), S(  36,  -29), S(  66,  -15), S(  37,   25), S(  27,   26),
		S( -12,   28), S( -15,   20), S(  -4,    5), S(  -7,   -9), S(  10,   -8), S(  18,  -12), S(  -2,    8), S(  11,    4),
		S( -15,    9), S( -19,    9), S(  -5,   -3), S(   6,   -7), S(   6,   -5), S(  11,  -10), S(  -9,    0), S(   3,   -7),
		S( -17,    5), S( -14,    3), S(  -7,   -3), S(  -3,   -1), S(  12,    1), S(   6,   -5), S(  15,   -7), S(   8,   -9),
		S( -12,    7), S( -11,    5), S(  -4,    0), S(  -4,    0), S(   8,    8), S(  28,   -5), S(  26,   -9), S(   5,  -11),
		S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)
	},
	{
		S(-103,  -32), S(-107,   -4), S( -55,    3), S( -19,   -7), S(   9,   -4), S( -34,  -26), S( -82,   -3), S( -62,  -50),
		S( -18,   -2), S( -12,    6), S(   2,    3), S(  22,    0), S(  23,   -9), S(  58,  -20), S(   1,   -1), S(  22,  -17),
		S(  -4,   -4), S(  12,    0), S(  25,    9), S(  40,    9), S(  58,   -1), S(  87,  -17), S(  35,   -9), S(  41,  -17),
		S(   5,    9), S(  14,    8), S(  29,   15), S(  54,   18), S(  45,   13), S(  55,   11), S(  31,    6), S(  43,   -1),
		S(  -5,   14), S(   3,    3), S(  12,   17), S(  24,   19), S(  25,   23), S(  25,    9), S(  27,    2), S(   8,   10),
		S( -21,    0), S( -10,   -1), S(  -3,    2), S(  -1,   16), S(  12,   14), S(   1,   -4), S(  11,   -4), S(  -5,    1),
		S( -26,    3), S( -23,    5), S( -15,   -3), S(   2,    0), S(   0,   -1), S(  -1,   -4), S(  -3,   -3), S(  -6,   15),
		S( -63,   15), S( -14,   -5), S( -33,   -3), S( -20,   -2), S( -15,    2), S(  -9,   -8), S( -12,    1), S( -34,   10)
	},
	{
		S( -15,    5), S( -48,   11), S( -39,    3), S( -78,   13), S( -62,    7), S( -49,   -3), S( -28,    3), S( -45,   -1),
		S( -11,   -7), S( -18,   -2), S( -17,   -2), S( -20,   -2), S(  -7,  -10), S(  -3,   -9), S( -19,   -2), S(   8,  -11),
		S(   2,   10), S(  11,   -2), S(   2,    0), S(  13,   -9), S(  11,   -5), S(  47,   -4), S(  34,   -5), S(  32,    6),
		S( -11,    8), S(  -2,    9), S(   5,    0), S(  19,    9), S(  11,    1), S(  14,    4), S(  -3,    8), S(  -3,    6),
		S(  -5,    4), S( -11,    7), S(   0,    5), S(  11,    4), S(  12,    1), S(  -2,    1), S(  -5,    6), S(   8,   -4),
		S(   2,    5), S(  11,    3), S(   5,    2), S(   9,    4), S(  10,    8), S(   8,    2), S(  11,   -3), S(  18,    0),
		S(  25,   10), S(   8,   -9), S(  19,  -11), S(   2,    0), S(   9,    2), S(  17,   -7), S(  27,   -3), S(  22,   -4),
		S(  11,   -1), S(  27,    5), S(  11,   -1), S(  -3,    3), S(   2,    0), S(   2,   10), S(  21,   -9), S(  27,  -11)
	},
	{
		S(  -2,   16), S( -13,   22), S( -14,   29), S( -16,   25), S(   3,   19), S(  27,   13), S(  24,   15), S(  41,    8),
		S( -13,   13), S( -11,   20), S(   1,   22), S(  17,   11), S(   9,   11), S(  31,    5), S(  38,    1), S(  59,   -8),
		S( -21,   11), S(   6,    7), S(  -1,    9), S(   2,    5), S(  29,   -5), S(  48,  -14), S(  90,  -18), S(  64,  -20),
		S( -21,   13), S(  -8,    8), S( -12,   14), S(  -8,    9), S(   0,   -4), S(  12,   -9), S(  31,  -10), S(  25,  -12),
		S( -30,    8), S( -32,   10), S( -26,    8), S( -20,    6), S( -16,    3), S( -18,    1), S(  12,   -9), S(  -2,  -10),
		S( -33,    4), S( -30,    1), S( -27,   -2), S( -23,    0), S( -13,   -6), S(  -5,  -14), S(  27,  -29), S(   5,  -24),
		S( -32,   -2), S( -29,    0), S( -19,   -1), S( -17,   -3), S( -10,  -10), S(  -2,  -15), S(  12,  -22), S( -16,  -14),
		S( -15,    2), S( -14,   -1), S( -11,    3), S(  -4,   -3), S(   4,  -10), S(   2,   -4), S(  10,  -13), S(  -9,  -12)
	},
	{
		S( -11,  -11), S( -33,   16), S( -15,   35), S(  14,   24), S(  11,   23), S(  28,   14), S(  67,  -36), S(  27,   -7),
		S(  -5,  -21), S( -33,    7), S( -32,   39), S( -42,   60), S( -33,   74), S(   2,   32), S(  -9,   24), S(  51,   11),
		S(   0,  -15), S(  -8,   -8), S( -15,   24), S(  -5,   33), S(   6,   43), S(  50,   23), S(  61,   -9), S(  64,    2),
		S( -15,   -2), S( -12,    4), S( -16,   13), S( -14,   28), S( -15,   45), S(   0,   36), S(  11,   37), S(  19,   23),
		S(  -8,  -11), S( -18,   10), S( -17,   11), S( -12,   24), S( -11,   24), S( -11,   23), S(   2,   16), S(  10,   19),
		S( -11,  -23), S(  -7,  -10), S( -13,    4), S( -12,    3), S( -10,    8), S(  -3,    3), S(   9,  -11), S(  11,  -17),
		S(  -4,  -27), S(  -9,  -26), S(  -1,  -28), S(   1,  -22), S(  -1,  -17), S(   7,  -41), S(  16,  -67), S(  31,  -81),
		S( -12,  -26), S(  -7,  -28), S(  -3,  -29), S(   2,  -16), S(   0,  -32), S( -12,  -30), S(   8,  -49), S(  10,  -57)
	},
	{
		S(  71, -107), S(  64,  -57), S(  78,  -42), S( -47,    3), S(  -9,  -10), S(  46,  -12), S(  99,  -22), S( 220, -132),
		S( -62,  -11), S(  -6,   16), S( -41,   28), S(  56,   13), S(  10,   30), S(   6,   44), S(  48,   30), S(  24,    0),
		S( -71,    1), S(  37,   21), S( -26,   39), S( -46,   50), S( -11,   51), S(  65,   42), S(  42,   40), S(   2,   10),
		S( -37,   -9), S( -48,   25), S( -67,   45), S(-111,   58), S(-104,   59), S( -63,   53), S( -56,   40), S( -84,   15),
		S( -44,  -18), S( -45,   13), S( -76,   37), S(-108,   53), S(-105,   52), S( -67,   38), S( -70,   26), S(-101,   10),
		S(   3,  -29), S(  20,   -4), S( -38,   19), S( -54,   31), S( -48,   31), S( -44,   22), S(  -2,    3), S( -20,   -9),
		S(  88,  -46), S(  47,  -18), S(  31,   -7), S(  -3,    4), S(  -8,    8), S(  12,   -1), S(  58,  -19), S(  65,  -37),
		S(  78,  -83), S( 104,  -63), S(  80,  -44), S( -11,  -25), S(  46,  -43), S(  12,  -27), S(  82,  -54), S(  81,  -84)
	}
};

constexpr InitialParam MOBILITY[4][28] = {
	{S(-134, -108), S( -35,  -51), S( -12,  -23), S(  -3,   -1), S(   6,   10), S(   8,   21), S(  16,   25), S(  26,   28), S(  35,   26)},
	{S( -34, -180), S( -54,  -92), S( -21,  -44), S( -11,  -20), S(   2,   -8), S(   9,    1), S(  16,   13), S(  23,   18), S(  27,   25), S(  32,   28), S(  36,   32), S(  50,   26), S(  62,   29), S(  76,   19)},
	{S( -62, -144), S( -41,  -87), S( -21,  -39), S( -13,  -22), S(  -7,  -13), S(  -4,   -6), S(  -1,   -0), S(   2,    6), S(   6,    7), S(  12,   10), S(  16,   14), S(  20,   18), S(  26,   21), S(  32,   21), S(  33,   22)},
	{S(   0,    0), S(   0,    0), S(-272,  -84), S( -17, -283), S( -46, -112), S( -15,  -94), S(  -8,  -77), S(  -3,  -57), S(   1,  -39), S(   3,  -12), S(   7,   -6), S(  12,    1), S(  15,   10), S(  20,   11), S(  22,   15), S(  24,   24), S(  25,   27), S(  25,   37), S(  26,   42), S(  27,   47), S(  33,   51), S(  41,   44), S(  55,   45), S(  70,   35), S(  81,   37), S( 194,  -17), S( 123,   11), S(  95,   16)}
};

constexpr InitialParam THREATS[6][6] = {
	{S(  21,   -2), S(  39,    3), S(  43,   35), S(  60,  -16), S(  44,  -41), S(   0,    0)},
	{S(  -6,    5), S(   0,    0), S(  22,   21), S(  46,  -13), S(  22,  -41), S(   0,    0)},
	{S(   3,   15), S(  21,   22), S(   0,    0), S(  28,    2), S(  36,   62), S(   0,    0)},
	{S(  -7,   11), S(   4,   15), S(  13,   10), S(   0,    0), S(  42,   -6), S(   0,    0)},
	{S(  -2,    8), S(   1,    7), S(  -1,   23), S(   1,   -2), S(   0,    0), S(   0,    0)},
	{S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)}
};

constexpr InitialParam PASSED_PAWN[8] = {S(   0,    0), S(  -5,   13), S(  -9,   17), S(  -9,   40), S(  11,   62), S(   3,  125), S(  20,  108), S(   0,    0)};
constexpr InitialParam ISOLATED_PAWN[8] = {S( -10,  -10), S(  -7,  -15), S( -18,  -16), S( -17,  -21), S( -20,  -24), S( -16,  -13), S(  -9,  -15), S( -21,  -10)};

constexpr InitialParam BISHOP_PAIR = S(  19,   60);
constexpr InitialParam ROOK_OPEN[2] = {S(  29,    7), S(  17,    2)};

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
    addEvalParamArray2D(params, PSQT);
    for (int i = 0; i < 6; i++)
        for (int j = (i == 0 ? 8 : 0); j < (i == 0 ? 56 : 64); j++)
        {
            params[i * 64 + j].mg += MATERIAL[i][0];
            params[i * 64 + j].eg += MATERIAL[i][1];
        }
    addEvalParamArray(params, std::span<const InitialParam>(MOBILITY[0], MOBILITY[0] + 9));
    addEvalParamArray(params, std::span<const InitialParam>(MOBILITY[1], MOBILITY[1] + 14));
    addEvalParamArray(params, std::span<const InitialParam>(MOBILITY[2], MOBILITY[2] + 15));
    addEvalParamArray(params, std::span<const InitialParam>(MOBILITY[3], MOBILITY[3] + 28));
    addEvalParamArray2D(params, THREATS);
    addEvalParamArray(params, PASSED_PAWN);
    addEvalParamArray(params, ISOLATED_PAWN);
    addEvalParam(params, BISHOP_PAIR);
    addEvalParamArray(params, ROOK_OPEN);
    return params;
}

EvalParams EvalFn::getMaterialParams()
{
    EvalParams params = getInitialParams();
    std::fill(params.begin(), params.end(), EvalParam{0, 0});

    for (int i = 0; i < 6; i++)
        for (int j = (i == 0 ? 8 : 0); j < (i == 0 ? 56 : 64); j++)
        {
            params[i * 64 + j].mg += MATERIAL[i][0];
            params[i * 64 + j].eg += MATERIAL[i][1];
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
    state.ss << "constexpr PackedScore PSQT[6][64] = {\n";
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
    state.ss << "};\n";
}

void printMaterial(PrintState& state)
{
    state.ss << "constexpr PackedScore MATERIAL[6] = {";
    for (int j = 0; j < 5; j++)
    {
        printSingle<4>(state);
        state.ss << ", ";
    }
    state.ss << "S(0, 0)};\n";
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

template<int ALIGN_SIZE>
void printArray2D(PrintState& state, int outerLen, int innerLen)
{
    state.ss << "{\n";
    for (int i = 0; i < outerLen; i++)
    {
        state.ss << '\t';
        printArray<ALIGN_SIZE>(state, innerLen);
        if (i != outerLen - 1)
            state.ss << ',';
        state.ss << '\n';
    }
    state.ss << "}";
}

template<int ALIGN_SIZE>
void printMobility(PrintState& state)
{
    state.ss << "constexpr PackedScore MOBILITY[4][28] = {\n\t";
    printArray<ALIGN_SIZE>(state, 9);
    state.ss << ",\n\t";

    printArray<ALIGN_SIZE>(state, 14);
    state.ss << ",\n\t";

    printArray<ALIGN_SIZE>(state, 15);
    state.ss << ",\n\t";

    printArray<ALIGN_SIZE>(state, 28);
    state.ss << "\n};\n";
}

template<int ALIGN_SIZE>
void printRestParams(PrintState& state)
{
    state.ss << "constexpr PackedScore THREATS[6][6] = ";
    printArray2D<ALIGN_SIZE>(state, 6, 6);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore PASSED_PAWN[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore ISOLATED_PAWN[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore BISHOP_PAIR = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore ROOK_OPEN[2] = ";
    printArray<ALIGN_SIZE>(state, 2);
    state.ss << ";\n";
}

void EvalFn::printEvalParams(const EvalParams& params, std::ostream& os)
{
    PrintState state{params, 0};
    printPSQTs<0>(state);
    printMobility<0>(state);
    printRestParams<0>(state);
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
    printRestParams<4>(state);
    os << state.ss.str() << std::endl;
}
