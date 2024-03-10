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

    TraceElem threats[6][6];

    TraceElem passedPawn[8];
    TraceElem isolatedPawn[8];
    TraceElem pawnStorm[3][8];

    TraceElem bishopPair;
    TraceElem openRook[2];
};

// literally copy pasted from stash
void evalKingFile(Trace& trace, Bitboard ourPawns, File file, Square theirKing, Color us)
{
    File kingFile = theirKing.file();
    int idx = (kingFile == file) ? 1 : (kingFile >= File::FILE_E) == (kingFile < file) ? 0 : 2;

    Bitboard filePawns = ourPawns & Bitboard(file);
    int rankDist = filePawns ?
        std::abs(Square(us == Color::WHITE ? filePawns.lsb() : filePawns.msb()).rank() - theirKing.rank()) :
        7;
    trace.pawnStorm[idx][rankDist][static_cast<int>(us)]++;
}

Trace getTrace(const chess::Board& board)
{
    Trace trace = {};

    std::array<Bitboard, 2> pawns = {
        board.pieces(PieceType::PAWN, Color::WHITE),
        board.pieces(PieceType::PAWN, Color::BLACK)
    };

    std::array<Bitboard, 2> pawnAttacks = {
        attacks::pawnLeftAttacks<Color::WHITE>(pawns[0]) | attacks::pawnRightAttacks<Color::WHITE>(pawns[0]),
        attacks::pawnLeftAttacks<Color::BLACK>(pawns[1]) | attacks::pawnRightAttacks<Color::BLACK>(pawns[1])
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

    for (Color us : {Color::WHITE, Color::BLACK})
    {
        for (int file = 0; file < 8; file++)
        {
            evalKingFile(trace, pawns[static_cast<int>(us)], File(file), board.kingSq(~us), us);
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
    addCoefficientArray2D(trace.pawnStorm);

    addCoefficient(trace.bishopPair);
    addCoefficientArray(trace.openRook);
    return {pos, m_Coefficients.size()};
}

using InitialParam = std::array<int, 2>;

#define S(mg, eg) {mg, eg}

constexpr InitialParam MATERIAL[6] = {S(  58,   82), S( 273,  341), S( 279,  353), S( 375,  626), S( 800, 1188), S(0, 0)};

constexpr InitialParam PSQT[6][64] = {
	{
		S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
		S(  34,  109), S(  39,  110), S(  22,  108), S(  58,   58), S(  32,   59), S(  24,   67), S( -56,  118), S( -22,  112),
		S(   4,   43), S(   0,   59), S(  24,   16), S(  28,  -18), S(  35,  -21), S(  69,   -7), S(  39,   41), S(  34,   26),
		S( -16,   24), S( -11,   29), S(  -4,    7), S(  -7,   -6), S(  12,   -6), S(  22,   -7), S(   8,   21), S(  24,   -1),
		S( -22,    4), S( -18,   17), S(  -9,   -2), S(  -1,   -5), S(   3,   -5), S(  11,   -8), S(   0,   10), S(  18,  -15),
		S( -26,   -1), S( -15,    9), S( -13,   -2), S( -12,    1), S(   6,    1), S(   4,   -4), S(  20,    1), S(  22,  -20),
		S( -21,    0), S( -12,   11), S( -10,    0), S( -13,    1), S(   4,    7), S(  26,   -6), S(  32,   -2), S(  20,  -23),
		S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
	},
	{
		S(-105,  -33), S(-105,   -6), S( -57,    4), S( -20,   -8), S(  10,   -4), S( -34,  -26), S( -83,   -4), S( -64,  -53),
		S( -19,   -2), S( -12,    7), S(   1,    1), S(  22,   -1), S(  22,   -9), S(  56,  -22), S(   2,   -3), S(  22,  -20),
		S(  -6,   -4), S(  10,    0), S(  24,    9), S(  39,    9), S(  57,   -2), S(  85,  -18), S(  34,   -9), S(  41,  -17),
		S(   4,   10), S(  13,    8), S(  28,   15), S(  53,   18), S(  44,   12), S(  55,   11), S(  30,    5), S(  43,   -2),
		S(  -6,   14), S(   3,    4), S(  12,   17), S(  23,   19), S(  24,   23), S(  24,    9), S(  27,    2), S(   8,   10),
		S( -22,    0), S( -11,    0), S(  -4,    1), S(  -2,   16), S(  12,   14), S(   1,   -4), S(  10,   -3), S(  -6,    2),
		S( -26,    4), S( -23,    5), S( -15,   -2), S(   1,    1), S(  -1,   -1), S(  -2,   -4), S(  -4,   -3), S(  -6,   16),
		S( -62,   15), S( -15,   -6), S( -33,   -2), S( -21,   -2), S( -16,    3), S( -10,   -7), S( -14,    1), S( -34,   11),
	},
	{
		S( -16,    5), S( -48,   10), S( -39,    2), S( -76,   11), S( -61,    6), S( -46,   -5), S( -28,   -1), S( -45,   -4),
		S( -11,   -8), S( -18,   -3), S( -17,   -3), S( -19,   -3), S(  -7,  -10), S(  -3,  -11), S( -17,   -3), S(   9,  -12),
		S(   1,    9), S(  10,   -2), S(   2,    0), S(  13,   -8), S(  11,   -5), S(  47,   -4), S(  35,   -6), S(  33,    4),
		S( -12,    6), S(  -3,    9), S(   4,    1), S(  19,    9), S(  11,    1), S(  13,    5), S(  -3,    8), S(  -1,    4),
		S(  -5,    4), S( -11,    8), S(   0,    6), S(  11,    4), S(  12,    2), S(  -2,    2), S(  -5,    5), S(   8,   -4),
		S(   2,    5), S(  11,    3), S(   5,    3), S(   9,    5), S(  10,    9), S(   8,    2), S(  11,   -3), S(  18,   -1),
		S(  25,    9), S(   8,   -9), S(  19,  -11), S(   1,    0), S(   9,    2), S(  17,   -7), S(  27,   -5), S(  22,   -5),
		S(  12,   -2), S(  27,    5), S(  10,   -1), S(  -3,    3), S(   2,   -1), S(   2,   10), S(  20,   -9), S(  29,  -14),
	},
	{
		S(  -7,   19), S( -12,   22), S( -12,   29), S( -15,   25), S(   4,   17), S(  29,   11), S(  20,   16), S(  38,    9),
		S( -15,   14), S( -10,   20), S(   2,   22), S(  19,   10), S(  10,   10), S(  33,    4), S(  38,    1), S(  56,   -7),
		S( -23,   11), S(   6,    7), S(  -1,    8), S(   2,    3), S(  28,   -7), S(  49,  -15), S(  88,  -19), S(  61,  -21),
		S( -23,   13), S( -10,    7), S( -12,   13), S(  -8,    8), S(   0,   -4), S(  14,  -10), S(  29,  -10), S(  24,  -13),
		S( -31,    8), S( -33,    9), S( -25,    8), S( -20,    6), S( -16,    3), S( -17,    0), S(  13,  -10), S(  -2,  -11),
		S( -33,    4), S( -30,    1), S( -26,   -1), S( -23,    0), S( -13,   -6), S(  -5,  -14), S(  27,  -30), S(   6,  -25),
		S( -33,   -2), S( -29,    0), S( -19,   -1), S( -17,   -3), S( -10,  -10), S(  -1,  -15), S(  11,  -22), S( -18,  -13),
		S( -15,    3), S( -15,   -1), S( -11,    4), S(  -4,   -3), S(   3,  -10), S(   2,   -4), S(   7,  -11), S( -11,   -9),
	},
	{
		S( -11,  -11), S( -33,   16), S( -15,   35), S(  14,   25), S(   9,   25), S(  27,   13), S(  65,  -38), S(  27,  -10),
		S(  -6,  -21), S( -33,    6), S( -32,   38), S( -41,   60), S( -33,   74), S(   2,   29), S(  -8,   17), S(  50,    6),
		S(  -1,  -16), S(  -9,   -9), S( -14,   23), S(  -6,   32), S(   6,   41), S(  51,   20), S(  64,  -14), S(  66,   -2),
		S( -15,   -3), S( -12,    4), S( -16,   12), S( -15,   28), S( -13,   44), S(   1,   34), S(  12,   35), S(  21,   20),
		S(  -8,  -11), S( -17,   10), S( -16,   10), S( -11,   23), S( -11,   24), S( -10,   22), S(   3,   16), S(  11,   18),
		S( -10,  -22), S(  -7,  -10), S( -13,    5), S( -12,    3), S(  -9,    8), S(  -3,    4), S(  10,  -12), S(  11,  -15),
		S(  -3,  -26), S(  -9,  -25), S(  -1,  -28), S(   1,  -21), S(   0,  -16), S(   8,  -40), S(  16,  -67), S(  30,  -79),
		S( -11,  -25), S(  -7,  -27), S(  -3,  -28), S(   3,  -16), S(   1,  -31), S( -12,  -28), S(   8,  -50), S(  10,  -54),
	},
	{
		S(  28,  -73), S(  58,  -43), S(  98,  -44), S(   5,  -16), S(  31,  -30), S(  45,  -15), S(  82,   -9), S( 176,  -99),
		S(-108,   16), S( -21,   22), S( -16,   13), S( 104,  -17), S(  52,   -6), S(  15,   28), S(  24,   36), S( -37,   30),
		S(-138,   25), S(  22,   19), S(   3,   14), S(  10,    7), S(  47,    2), S(  80,   14), S(   6,   39), S( -70,   32),
		S( -92,   11), S( -61,   23), S( -39,   21), S( -46,   14), S( -48,    8), S( -52,   27), S( -91,   37), S(-151,   36),
		S( -88,    7), S( -61,   20), S( -68,   25), S( -66,   21), S( -71,   20), S( -72,   25), S( -97,   33), S(-152,   34),
		S( -28,    1), S(   8,   11), S( -31,   18), S( -20,   13), S( -25,   14), S( -44,   22), S( -20,   18), S( -52,   20),
		S(  76,  -17), S(  50,   -1), S(  45,   -2), S(  29,   -6), S(  20,   -1), S(  20,    4), S(  54,   -1), S(  52,   -7),
		S(  74,  -55), S( 110,  -46), S(  96,  -39), S(  24,  -33), S(  72,  -50), S(  25,  -21), S(  85,  -36), S(  78,  -55),
	},
};

constexpr InitialParam MOBILITY[4][28] = {
	{S(-135, -105), S( -34,  -51), S( -12,  -23), S(  -3,   -1), S(   6,   10), S(   9,   21), S(  17,   25), S(  26,   28), S(  36,   25)},
	{S( -38, -172), S( -54,  -88), S( -20,  -41), S( -10,  -18), S(   3,   -6), S(  10,    3), S(  18,   14), S(  24,   19), S(  28,   26), S(  34,   28), S(  38,   33), S(  52,   26), S(  62,   29), S(  77,   19)},
	{S( -44, -159), S( -40,  -93), S( -23,  -38), S( -15,  -21), S(  -8,  -12), S(  -5,   -4), S(  -3,    1), S(   0,    7), S(   4,    8), S(  10,   11), S(  14,   15), S(  18,   18), S(  24,   21), S(  29,   21), S(  30,   21)},
	{S(   0,    0), S(   0,    0), S(-363, -124), S( -16, -280), S( -47, -110), S( -17,  -89), S( -10,  -71), S(  -6,  -52), S(  -2,  -34), S(   1,   -7), S(   5,   -1), S(  10,    5), S(  13,   14), S(  18,   14), S(  21,   19), S(  23,   27), S(  24,   30), S(  24,   39), S(  25,   45), S(  26,   49), S(  31,   54), S(  39,   46), S(  54,   46), S(  69,   37), S(  77,   39), S( 189,  -14), S( 111,   17), S( 121,    0)}
};

constexpr InitialParam THREATS[6][6] = {
	{S(  22,    2), S(  40,    3), S(  43,   35), S(  60,  -16), S(  45,  -41), S(   0,    0)},
	{S(  -6,    5), S(   0,    0), S(  22,   21), S(  46,  -13), S(  21,  -41), S(   0,    0)},
	{S(   3,   15), S(  21,   21), S(   0,    0), S(  29,    1), S(  35,   63), S(   0,    0)},
	{S(  -7,   12), S(   4,   14), S(  13,   11), S(   0,    0), S(  41,   -5), S(   0,    0)},
	{S(  -2,   10), S(   1,    7), S(  -1,   23), S(   1,   -3), S(   0,    0), S(   0,    0)},
	{S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)}
};

constexpr InitialParam PASSED_PAWN[8] = {S(   0,    0), S(  -5,    5), S( -11,   11), S( -12,   36), S(   9,   60), S(   4,  121), S(  27,   97), S(   0,    0)};
constexpr InitialParam ISOLATED_PAWN[8] = {S(  -3,   -1), S(  -6,  -16), S( -16,  -11), S( -14,  -19), S( -18,  -19), S( -14,   -8), S(  -7,  -15), S( -23,    5)};
constexpr InitialParam PAWN_STORM[3][8] = {
	{S(  22,  -32), S(  14,  -16), S(  16,   -1), S(   8,    3), S(   7,    4), S(   7,    6), S(   8,    4), S(  21,  -27)},
	{S(   0,    0), S( -36,  -28), S(  16,    8), S(  -8,   11), S(  -4,   14), S(  -1,   18), S(  -1,   17), S(   8,  -13)},
	{S( -19,   10), S( -13,   15), S(  -4,   17), S(  -3,   18), S(   2,   16), S(   5,   17), S(   4,   17), S( -12,   -9)}
};

constexpr InitialParam BISHOP_PAIR = S(  19,   61);
constexpr InitialParam ROOK_OPEN[2] = {S(  29,    9), S(  19,    6)};

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
    addEvalParamArray2D(params, PAWN_STORM);
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

    state.ss << "constexpr PackedScore PAWN_STORM[3][8] = ";
    printArray2D<ALIGN_SIZE>(state, 3, 8);
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
