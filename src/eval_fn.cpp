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
    TraceElem pawnShield[3][8];

    TraceElem bishopPair;
    TraceElem openRook[2];
};

// literally copy pasted from stash
void evalKingFile(Trace& trace, Bitboard ourPawns, File file, Square theirKing, Square ourKing, Color us)
{
    Bitboard filePawns = ourPawns & Bitboard(file);
    // pawn storm - our pawns their king
    {
        File kingFile = theirKing.file();
        int idx = (kingFile == file) ? 1 : (kingFile >= File::FILE_E) == (kingFile < file) ? 0 : 2;

        int rankDist = filePawns ?
            std::abs(Square(us == Color::BLACK ? filePawns.lsb() : filePawns.msb()).rank() - theirKing.rank()) :
            7;
        trace.pawnStorm[idx][rankDist][static_cast<int>(us)]++;
    }
    // pawn shield - our pawns our king
    {
        File kingFile = ourKing.file();
        int idx = (kingFile == file) ? 1 : (kingFile >= File::FILE_E) == (kingFile < file) ? 0 : 2;

        int rankDist = filePawns ?
            std::abs(Square(us == Color::WHITE ? filePawns.lsb() : filePawns.msb()).rank() - ourKing.rank()) :
            7;
        trace.pawnShield[idx][rankDist][static_cast<int>(us)]++;
    }
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
            evalKingFile(trace, pawns[static_cast<int>(us)], File(file), board.kingSq(~us), board.kingSq(us), us);
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
    addCoefficientArray2D(trace.pawnShield);

    addCoefficient(trace.bishopPair);
    addCoefficientArray(trace.openRook);
    return {pos, m_Coefficients.size()};
}

using InitialParam = std::array<int, 2>;

#define S(mg, eg) {mg, eg}

constexpr InitialParam MATERIAL[6] = {S(  53,   85), S( 275,  341), S( 280,  353), S( 381,  625), S( 807, 1187), S(0, 0)};

constexpr InitialParam PSQT[6][64] = {
	{
		S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
		S(  35,  100), S(  43,  102), S(  27,   99), S(  63,   51), S(  39,   51), S(  31,   56), S( -35,  107), S( -27,   99),
		S(   7,   48), S(   3,   65), S(  26,   21), S(  33,  -12), S(  38,  -15), S(  69,   -3), S(  35,   46), S(  15,   32),
		S( -10,   25), S(  -3,   30), S(   4,    7), S(   1,   -5), S(  18,   -4), S(  26,   -7), S(  11,   22), S(   4,    2),
		S( -18,    3), S( -11,   16), S(  -3,   -4), S(   7,   -7), S(   7,   -5), S(  12,  -10), S(   2,    9), S(  -6,  -13),
		S( -23,   -4), S( -10,    7), S(  -8,   -6), S(  -5,   -3), S(   6,   -1), S(   3,   -7), S(  14,    0), S(  -7,  -18),
		S( -19,   -3), S(  -8,    8), S(  -6,   -3), S(  -8,   -3), S(   2,    6), S(  24,   -9), S(  22,   -1), S(  -9,  -19),
		S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
	},
	{
		S(-108,  -31), S(-103,   -5), S( -59,    5), S( -22,   -7), S(   9,   -4), S( -38,  -25), S( -81,   -4), S( -73,  -49),
		S( -20,    0), S( -11,    7), S(   2,    2), S(  22,   -1), S(  23,   -9), S(  56,  -22), S(  10,   -3), S(  21,  -19),
		S(  -5,   -4), S(  12,    0), S(  25,    9), S(  39,    9), S(  57,   -1), S(  89,  -18), S(  33,   -8), S(  41,  -17),
		S(   4,   10), S(  13,    8), S(  29,   15), S(  53,   18), S(  45,   13), S(  56,   11), S(  30,    5), S(  43,   -2),
		S(  -4,   14), S(   3,    4), S(  12,   17), S(  23,   19), S(  25,   24), S(  24,    9), S(  28,    2), S(   9,   11),
		S( -21,    0), S( -10,    0), S(  -4,    2), S(  -2,   16), S(  12,   14), S(   1,   -3), S(  11,   -3), S(  -6,    3),
		S( -25,    4), S( -23,    6), S( -15,   -2), S(   1,    1), S(   0,   -1), S(  -1,   -4), S(   0,   -3), S(  -4,   17),
		S( -58,   15), S( -16,   -5), S( -33,   -2), S( -21,   -1), S( -15,    3), S( -10,   -6), S( -15,    3), S( -32,   13),
	},
	{
		S( -18,    6), S( -47,   10), S( -39,    2), S( -76,   10), S( -61,    6), S( -51,   -3), S( -28,    0), S( -44,   -4),
		S( -15,   -7), S( -17,   -3), S( -16,   -3), S( -18,   -4), S(  -7,  -11), S(  -1,  -11), S( -19,   -1), S(  -1,   -9),
		S(   1,    9), S(   9,   -2), S(   2,    0), S(  13,   -8), S(  11,   -5), S(  47,   -5), S(  33,   -5), S(  31,    5),
		S( -12,    6), S(  -2,    9), S(   4,    1), S(  19,    9), S(  11,    1), S(  13,    4), S(  -2,    7), S(   1,    3),
		S(  -3,    4), S( -11,    8), S(   0,    6), S(  11,    5), S(  12,    1), S(  -1,    2), S(  -3,    5), S(   9,   -4),
		S(   2,    6), S(  12,    3), S(   5,    3), S(  10,    5), S(  11,    9), S(   9,    1), S(  12,   -2), S(  19,   -1),
		S(  25,    9), S(   9,   -9), S(  20,  -11), S(   2,    0), S(   9,    2), S(  17,   -6), S(  30,   -6), S(  23,   -4),
		S(  12,   -2), S(  27,    6), S(  10,   -1), S(  -2,    3), S(   3,   -1), S(   2,   11), S(  21,   -9), S(  32,  -15),
	},
	{
		S( -10,   20), S( -12,   22), S( -12,   29), S( -14,   25), S(   5,   17), S(  31,   10), S(  24,   14), S(  38,    9),
		S( -15,   14), S(  -9,   20), S(   4,   21), S(  20,   10), S(  13,   10), S(  38,    2), S(  44,   -1), S(  54,   -7),
		S( -22,   10), S(   7,    6), S(   1,    8), S(   5,    2), S(  31,   -8), S(  51,  -16), S(  82,  -17), S(  53,  -20),
		S( -23,   12), S(  -8,    6), S(  -9,   11), S(  -5,    6), S(   2,   -5), S(  14,  -11), S(  26,  -10), S(  17,  -12),
		S( -31,    7), S( -31,    8), S( -24,    7), S( -17,    4), S( -14,    2), S( -17,    0), S(  10,  -10), S(  -5,  -10),
		S( -35,    4), S( -30,    0), S( -25,   -2), S( -21,   -1), S( -12,   -6), S(  -5,  -13), S(  25,  -28), S(   3,  -23),
		S( -34,   -2), S( -28,    0), S( -17,   -2), S( -15,   -3), S(  -9,  -10), S(  -1,  -15), S(  11,  -22), S( -21,  -13),
		S( -16,    3), S( -15,   -1), S( -11,    3), S(  -3,   -4), S(   3,  -10), S(   1,   -5), S(   1,   -9), S( -14,   -7),
	},
	{
		S( -19,   -7), S( -35,   16), S( -16,   35), S(  10,   27), S(   9,   25), S(  26,   13), S(  68,  -39), S(  23,   -8),
		S(  -7,  -20), S( -32,    6), S( -30,   38), S( -41,   61), S( -32,   75), S(   7,   27), S(   1,   14), S(  50,    7),
		S(  -2,  -15), S(  -9,   -9), S( -13,   22), S(  -5,   31), S(   6,   42), S(  49,   22), S(  64,  -13), S(  60,    2),
		S( -16,   -4), S( -12,    3), S( -14,   11), S( -16,   29), S( -13,   44), S(   2,   33), S(  12,   35), S(  21,   19),
		S(  -8,  -11), S( -18,   10), S( -16,   10), S( -11,   23), S( -10,   23), S(  -9,   22), S(   4,   17), S(  11,   19),
		S( -11,  -22), S(  -7,   -9), S( -12,    4), S( -12,    2), S(  -9,    6), S(  -2,    2), S(  11,  -11), S(  10,  -12),
		S(  -5,  -25), S(  -9,  -25), S(  -1,  -27), S(   1,  -22), S(   0,  -17), S(   7,  -41), S(  15,  -64), S(  26,  -75),
		S( -12,  -25), S(  -8,  -27), S(  -5,  -27), S(   1,  -15), S(   0,  -32), S( -12,  -28), S(   0,  -45), S(   8,  -55),
	},
	{
		S(  25,  -55), S(  64,  -30), S( 126,  -39), S(  44,  -12), S(  57,  -32), S(  43,   -8), S(  67,    3), S( 159,  -79),
		S(-116,   32), S(  -9,   30), S(  13,   15), S( 141,  -18), S(  87,   -9), S(  26,   31), S(  28,   43), S( -55,   46),
		S(-139,   31), S(  21,   22), S(  21,   12), S(  42,    2), S(  71,   -3), S(  89,   13), S(   2,   41), S( -69,   35),
		S( -86,    7), S( -55,   18), S( -27,   14), S( -23,    4), S( -32,   -1), S( -45,   19), S( -89,   32), S(-146,   31),
		S( -86,   -2), S( -57,   10), S( -56,   13), S( -48,    8), S( -61,    8), S( -68,   14), S( -94,   22), S(-152,   24),
		S( -39,   -5), S(  -2,    4), S( -34,   10), S( -20,    4), S( -24,    5), S( -49,   15), S( -29,   12), S( -60,   13),
		S(  57,  -19), S(  34,   -3), S(  27,   -4), S(  15,  -10), S(   5,   -5), S(  12,    0), S(  35,   -3), S(  40,  -11),
		S(  56,  -48), S(  84,  -38), S(  67,  -29), S(  11,  -26), S(  57,  -42), S(   5,  -12), S(  68,  -29), S(  67,  -51),
	},
};

constexpr InitialParam MOBILITY[4][28] = {
	{S(-131, -110), S( -33,  -53), S( -10,  -25), S(  -0,   -3), S(   9,    8), S(  11,   19), S(  19,   23), S(  29,   27), S(  38,   24)},
	{S( -36, -178), S( -50,  -92), S( -17,  -44), S(  -8,  -20), S(   5,   -8), S(  13,    2), S(  20,   13), S(  27,   18), S(  31,   25), S(  36,   27), S(  40,   32), S(  54,   25), S(  64,   29), S(  78,   18)},
	{S( -61, -133), S( -38,  -92), S( -19,  -39), S( -12,  -22), S(  -6,  -13), S(  -3,   -5), S(  -1,    1), S(   2,    7), S(   6,    8), S(  11,   11), S(  15,   15), S(  19,   18), S(  25,   20), S(  31,   21), S(  32,   20)},
	{S(   0,    0), S(   0,    0), S(-189,  -58), S( -18, -264), S( -42, -115), S( -12,  -95), S(  -6,  -75), S(  -2,  -55), S(   2,  -37), S(   4,  -10), S(   8,   -3), S(  12,    4), S(  15,   13), S(  20,   14), S(  23,   18), S(  24,   26), S(  26,   29), S(  26,   39), S(  27,   45), S(  28,   48), S(  34,   53), S(  43,   43), S(  56,   45), S(  71,   35), S(  77,   39), S( 185,  -13), S(  99,   23), S(  90,   16)}
};

constexpr InitialParam THREATS[6][6] = {
	{S(  22,    2), S(  40,    4), S(  43,   35), S(  60,  -16), S(  44,  -42), S(   0,    0)},
	{S(  -6,    5), S(   0,    0), S(  22,   21), S(  47,  -14), S(  22,  -41), S(   0,    0)},
	{S(   3,   15), S(  21,   21), S(   0,    0), S(  30,    0), S(  35,   64), S(   0,    0)},
	{S(  -7,   11), S(   3,   14), S(  12,   11), S(   0,    0), S(  41,   -5), S(   0,    0)},
	{S(  -2,    9), S(   1,    7), S(  -2,   24), S(   2,   -4), S(   0,    0), S(   0,    0)},
	{S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)}
};

constexpr InitialParam PASSED_PAWN[8] = {S(   0,    0), S(  -5,    5), S( -11,   11), S( -12,   36), S(  10,   60), S(   5,  121), S(  36,  112), S(   0,    0)};
constexpr InitialParam ISOLATED_PAWN[8] = {S(  -4,   -0), S(  -7,  -16), S( -16,  -10), S( -15,  -19), S( -17,  -20), S( -10,   -8), S(  -7,  -15), S(  -8,    4)};
constexpr InitialParam PAWN_STORM[3][8] = {
	{S(  23,  -35), S(  17,  -18), S(  18,   -3), S(  11,    2), S(   6,    3), S(   2,    7), S(   2,    4), S(   3,  -12)},
	{S(   0,    0), S( -50,  -33), S(  26,    2), S(   2,    5), S(  -3,   11), S(  -4,   16), S(  -6,   15), S(   4,   -4)},
	{S(  -5,    1), S(  -1,    7), S(   8,   10), S(   5,   11), S(   7,   10), S(   7,    9), S(   4,    8), S( -16,   -2)}
};
constexpr InitialParam PAWN_SHIELD[3][8] = {
	{S(   3,   15), S(   8,   10), S(   8,   14), S(  -2,   10), S( -15,    8), S( -18,   17), S( -21,   18), S( -13,    2)},
	{S(   0,    0), S(  23,    1), S(  15,    5), S(  -6,   -2), S( -19,   -1), S( -30,   12), S( -56,   21), S( -27,   -3)},
	{S(   6,   11), S(   3,    7), S(   1,    6), S(  -2,   -0), S(  -7,   -4), S(  -5,   -8), S( -10,  -15), S(  11,   -7)}
};

constexpr InitialParam BISHOP_PAIR = S(  20,   61);
constexpr InitialParam ROOK_OPEN[2] = {S(  27,   10), S(  15,    7)};

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
    addEvalParamArray2D(params, PAWN_SHIELD);
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

    state.ss << "constexpr PackedScore PAWN_SHIELD[3][8] = ";
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
