#include "eval_fn.h"
#include "sirius/attacks.h"

#include <sstream>
#include <iomanip>

template<typename T>
struct ColorArray : public std::array<T, 2>
{
    using std::array<T, 2>::operator[];

    T& operator[](Color p)
    {
        return (*this)[static_cast<int>(p)];
    }

    const T& operator[](Color p) const
    {
        return (*this)[static_cast<int>(p)];
    }
};

template<typename T>
struct PieceTypeArray : public std::array<T, 6>
{
    using std::array<T, 6>::operator[];

    T& operator[](PieceType p)
    {
        return (*this)[static_cast<int>(p)];
    }

    const T& operator[](PieceType p) const
    {
        return (*this)[static_cast<int>(p)];
    }
};

using TraceElem = ColorArray<int>;

struct Trace
{
    TraceElem psqt[6][64];

    TraceElem mobility[4][28];

    TraceElem threats[6][6];

    TraceElem passedPawn[8];
    TraceElem isolatedPawn[8];
    TraceElem pawnPhalanx[8];
    TraceElem defendedPawn[8];

    TraceElem pawnStorm[3][8];
    TraceElem pawnShield[3][8];
    TraceElem safeKnightCheck;
    TraceElem safeBishopCheck;
    TraceElem safeRookCheck;
    TraceElem safeQueenCheck;

    TraceElem bishopPair;
    TraceElem openRook[2];
};

struct EvalData
{
    ColorArray<Bitboard> mobilityArea;
    ColorArray<Bitboard> attacked;
    ColorArray<PieceTypeArray<Bitboard>> attackedBy;
};

#define TRACE_INC(traceElem) trace.traceElem[us]++
#define TRACE_ADD(traceElem, amount) trace.traceElem[us] += amount

template<Color us, PieceType piece>
void evaluatePieces(const Board& board, EvalData& evalData, Trace& trace)
{
    constexpr Color them = ~us;
    Bitboard ourPawns = board.getPieces(us, PieceType::PAWN);
    Bitboard theirPawns = board.getPieces(them, PieceType::PAWN);

    Bitboard pieces = board.getPieces(us, piece);
    if constexpr (piece == PieceType::BISHOP)
        if (pieces.multiple())
            TRACE_INC(bishopPair);

    while (pieces)
    {
        uint32_t sq = pieces.poplsb();
        Bitboard attacks = attacks::pieceAttacks<piece>(sq, board.getAllPieces());
        evalData.attackedBy[us][piece] |= attacks;
        evalData.attacked[us] |= attacks;

        TRACE_INC(mobility[static_cast<int>(piece) - static_cast<int>(PieceType::KNIGHT)][(attacks & evalData.mobilityArea[us]).popcount()]);

        Bitboard threats = attacks & board.getColor(them);
        while (threats)
            TRACE_INC(threats[static_cast<int>(piece)][static_cast<int>(getPieceType(board.getPieceAt(threats.poplsb())))]);

        Bitboard fileBB = Bitboard::fileBB(fileOf(sq));

        if constexpr (piece == PieceType::ROOK)
        {
            if ((ourPawns & fileBB).empty())
            {
                if ((theirPawns & fileBB).any())
                    TRACE_INC(openRook[1]);
                else
                    TRACE_INC(openRook[0]);
            }
        }
    }
}



template<Color us>
void evaluatePawns(const Board& board, Trace& trace)
{
    Bitboard ourPawns = board.getPieces(us, PieceType::PAWN);

    Bitboard pawns = ourPawns;
    while (pawns)
    {
        uint32_t sq = pawns.poplsb();
        if (board.isPassedPawn(sq))
            TRACE_INC(passedPawn[relativeRankOf<us>(sq)]);
        if (board.isIsolatedPawn(sq))
            TRACE_INC(isolatedPawn[fileOf(sq)]);
    }

    Bitboard phalanx = ourPawns & ourPawns.west();
    while (phalanx)
        TRACE_INC(pawnPhalanx[relativeRankOf<us>(phalanx.poplsb())]);

    // shift with opposite color is intentional
    Bitboard defended = ourPawns & attacks::pawnAttacks<us>(ourPawns);
    while (defended)
        TRACE_INC(defendedPawn[relativeRankOf<us>(defended.poplsb())]);

}

void evaluatePawns(const Board& board, Trace& trace)
{
    evaluatePawns<Color::WHITE>(board, trace);
    evaluatePawns<Color::BLACK>(board, trace);
}

// I'll figure out how to add the other pieces here later
template<Color us>
void evaluateThreats(const Board& board, const EvalData& evalData, Trace& trace)
{
    constexpr Color them = ~us;
    Bitboard threats = evalData.attackedBy[us][PieceType::PAWN] & board.getColor(them);
    while (threats)
        TRACE_INC(threats[static_cast<int>(PieceType::PAWN)][static_cast<int>(getPieceType(board.getPieceAt(threats.poplsb())))]);
}

template<Color us>
PackedScore evalKingPawnFile(uint32_t file, Bitboard ourPawns, Bitboard theirPawns, uint32_t theirKing, Trace& trace)
{
    uint32_t kingFile = fileOf(theirKing);
    {
        Bitboard filePawns = ourPawns & Bitboard::fileBB(file);
        // 4 = e file
        int idx = (kingFile == file) ? 1 : (kingFile >= 4) == (kingFile < file) ? 0 : 2;

        int rankDist = filePawns ?
            std::abs(rankOf(us == Color::WHITE ? filePawns.msb() : filePawns.lsb()) - rankOf(theirKing)) :
            7;
        TRACE_INC(trace.pawnStorm[idx][rankDist]);
    }
    {
        Bitboard filePawns = theirPawns & Bitboard::fileBB(file);
        // 4 = e file
        int idx = (kingFile == file) ? 1 : (kingFile >= 4) == (kingFile < file) ? 0 : 2;
        int rankDist = filePawns ?
            std::abs(rankOf(us == Color::WHITE ? filePawns.msb() : filePawns.lsb()) - rankOf(theirKing)) :
            7;
        TRACE_INC(trace.pawnShield[idx][rankDist]);
    }
}

template<Color us>
void evaluateKings(const Board& board, const EvalData& evalData, Trace& trace)
{
    constexpr Color them = ~us;
    Bitboard ourPawns = board.getPieces(us, PieceType::PAWN);
    Bitboard theirPawns = board.getPieces(them, PieceType::PAWN);

    uint32_t theirKing = board.getPieces(them, PieceType::KING).lsb();

    for (uint32_t file = 0; file < 8; file++)
        evalKingPawnFile<us>(file, ourPawns, theirPawns, theirKing, trace);

    Bitboard rookCheckSquares = attacks::rookAttacks(theirKing, board.getAllPieces());
    Bitboard bishopCheckSquares = attacks::rookAttacks(theirKing, board.getAllPieces());

    Bitboard knightChecks = evalData.attackedBy[us][PieceType::KNIGHT] & attacks::knightAttacks(theirKing);
    Bitboard bishopChecks = evalData.attackedBy[us][PieceType::BISHOP] & bishopCheckSquares;
    Bitboard rookChecks = evalData.attackedBy[us][PieceType::ROOK] & rookCheckSquares;
    Bitboard queenChecks = evalData.attackedBy[us][PieceType::QUEEN] & (bishopCheckSquares | rookCheckSquares);

    Bitboard safe = ~evalData.attacked[them];

    TRACE_ADD(safeKnightCheck, (knightChecks & safe).popcount());
    TRACE_ADD(safeBishopCheck, (bishopChecks & safe).popcount());
    TRACE_ADD(safeRookCheck, (rookChecks & safe).popcount());
    TRACE_ADD(safeQueenCheck, (queenChecks & safe).popcount());
}

void initEvalData(const Board& board, EvalData& evalData)
{
    Bitboard whitePawns = board.getPieces(Color::WHITE, PieceType::PAWN);
    Bitboard blackPawns = board.getPieces(Color::BLACK, PieceType::PAWN);
    Bitboard whitePawnAttacks = attacks::pawnAttacks<Color::WHITE>(whitePawns);
    Bitboard blackPawnAttacks = attacks::pawnAttacks<Color::BLACK>(blackPawns);

    evalData.mobilityArea[Color::WHITE] = ~blackPawnAttacks;
    evalData.attacked[Color::WHITE] = evalData.attackedBy[Color::WHITE][PieceType::PAWN] = whitePawnAttacks;
    evalData.mobilityArea[Color::BLACK] = ~whitePawnAttacks;
    evalData.attacked[Color::BLACK] = evalData.attackedBy[Color::BLACK][PieceType::PAWN] = blackPawnAttacks;
}

void evaluatePsqt(const Board& board, Trace& trace)
{
    for (Color c : {Color::WHITE, Color::BLACK})
        for (PieceType pt : {PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN, PieceType::KING})
        {
            Bitboard pieces = board.getPieces(c, pt);
            while (pieces.any())
            {
                uint32_t sq = pieces.poplsb();
                if (c == Color::WHITE)
                    sq ^= 56;
                trace.psqt[static_cast<int>(pt)][sq][c]++;
            }
        }
}

Trace getTrace(const Board& board)
{
    EvalData evalData = {};
    initEvalData(board, evalData);

    Trace trace = {};

    evaluatePsqt(board, trace);

    evaluatePieces<Color::WHITE, PieceType::KNIGHT>(board, evalData, trace);
    evaluatePieces<Color::BLACK, PieceType::KNIGHT>(board, evalData, trace);
    evaluatePieces<Color::WHITE, PieceType::BISHOP>(board, evalData, trace);
    evaluatePieces<Color::BLACK, PieceType::BISHOP>(board, evalData, trace);
    evaluatePieces<Color::WHITE, PieceType::ROOK>(board, evalData, trace);
    evaluatePieces<Color::BLACK, PieceType::ROOK>(board, evalData, trace);
    evaluatePieces<Color::WHITE, PieceType::QUEEN>(board, evalData, trace);
    evaluatePieces<Color::BLACK, PieceType::QUEEN>(board, evalData, trace);

    evaluateKings<Color::WHITE>(board, evalData, trace);
    evaluateKings<Color::BLACK>(board, evalData, trace);

    evaluatePawns(board, trace);
    evaluateThreats<Color::WHITE>(board, evalData, trace);
    evaluateThreats<Color::BLACK>(board, evalData, trace);

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

std::pair<size_t, size_t> EvalFn::getCoefficients(const Board& board)
{
    reset();
    size_t pos = m_Coefficients.size();
    Trace trace = getTrace(board);
    addCoefficientArray2D(trace.psqt);

    addCoefficientArray2D(trace.mobility);

    addCoefficientArray2D(trace.threats);

    addCoefficientArray(trace.passedPawn);
    addCoefficientArray(trace.isolatedPawn);
    addCoefficientArray(trace.pawnPhalanx);
    addCoefficientArray(trace.defendedPawn);

    addCoefficientArray2D(trace.pawnStorm);
    addCoefficientArray2D(trace.pawnShield);
    addCoefficient(trace.safeKnightCheck);
    addCoefficient(trace.safeBishopCheck);
    addCoefficient(trace.safeRookCheck);
    addCoefficient(trace.safeQueenCheck);

    addCoefficient(trace.bishopPair);
    addCoefficientArray(trace.openRook);
    return {pos, m_Coefficients.size()};
}

using InitialParam = std::array<int, 2>;

#define S(mg, eg) {mg, eg}

constexpr InitialParam MATERIAL[6] = {S(  48,   78), S( 277,  342), S( 282,  354), S( 385,  626), S( 812, 1192), S(0, 0)};

constexpr InitialParam PSQT[6][64] = {
	{
		S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
		S(  36,  107), S(  47,  103), S(  28,  105), S(  63,   55), S(  41,   57), S(  34,   63), S( -31,  112), S( -26,  108),
		S(  13,   49), S(   4,   64), S(  30,   17), S(  38,  -17), S(  44,  -21), S(  73,   -6), S(  35,   44), S(  22,   33),
		S(  -6,   25), S(  -2,   27), S(   3,    4), S(   3,  -10), S(  21,   -9), S(  25,  -10), S(  10,   19), S(  10,    1),
		S( -14,    4), S( -11,   18), S(  -4,   -4), S(   4,   -8), S(   7,   -6), S(  12,   -9), S(   3,   10), S(  -3,  -12),
		S( -23,   -4), S( -18,    7), S( -12,   -6), S(  -9,   -3), S(   0,    0), S(  -3,   -5), S(   6,    1), S(  -8,  -19),
		S( -13,    1), S(  -7,   15), S(  -2,    2), S(  -3,   -1), S(   4,   10), S(  29,   -3), S(  23,    5), S(  -3,  -15),
		S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
	},
	{
		S(-112,  -32), S(-107,   -6), S( -60,    5), S( -22,   -7), S(   7,   -4), S( -39,  -24), S( -83,   -3), S( -77,  -50),
		S( -23,   -1), S( -12,    7), S(   2,    2), S(  24,    0), S(  23,   -9), S(  56,  -22), S(  10,   -3), S(  19,  -19),
		S(  -5,   -4), S(  14,    0), S(  28,    9), S(  42,    8), S(  59,   -2), S(  91,  -18), S(  34,   -8), S(  40,  -17),
		S(   3,   10), S(  14,    8), S(  31,   14), S(  55,   17), S(  48,   12), S(  58,   11), S(  31,    5), S(  42,   -3),
		S(  -6,   13), S(   4,    3), S(  13,   17), S(  23,   19), S(  24,   23), S(  25,    9), S(  30,    1), S(   8,   10),
		S( -23,    0), S( -11,   -1), S(  -3,    2), S(  -1,   15), S(  12,   14), S(   2,   -2), S(  11,   -3), S(  -7,    2),
		S( -26,    2), S( -24,    6), S( -15,   -2), S(   0,    0), S(  -1,   -1), S(  -2,   -4), S(  -1,   -3), S(  -5,   16),
		S( -60,   13), S( -18,   -6), S( -33,   -3), S( -21,   -1), S( -16,    4), S( -10,   -7), S( -16,    2), S( -34,   12),
	},
	{
		S( -19,    6), S( -50,    9), S( -41,    1), S( -78,   10), S( -63,    6), S( -53,   -3), S( -27,   -1), S( -47,   -3),
		S( -16,   -7), S( -16,   -2), S( -17,   -3), S( -20,   -3), S(  -7,  -10), S(  -2,  -11), S( -19,    0), S(  -3,   -9),
		S(   1,    8), S(   9,   -1), S(   2,    1), S(  13,   -7), S(  10,   -4), S(  48,   -3), S(  31,   -5), S(  31,    5),
		S( -12,    6), S(  -3,    9), S(   3,    2), S(  18,   10), S(  10,    2), S(  12,    5), S(  -2,    7), S(   0,    3),
		S(  -3,    4), S( -11,    8), S(   0,    7), S(  10,    6), S(  11,    3), S(  -1,    3), S(  -4,    5), S(   9,   -4),
		S(   0,    5), S(  12,    3), S(   4,    3), S(   8,    5), S(   9,    9), S(   9,    2), S(  10,   -2), S(  18,   -2),
		S(  23,    7), S(   8,  -10), S(  19,  -11), S(   1,    0), S(   9,    1), S(  15,   -6), S(  30,   -7), S(  22,   -6),
		S(  12,   -4), S(  26,    5), S(   9,   -2), S(  -2,    2), S(   4,   -2), S(   2,   10), S(  19,   -8), S(  31,  -16),
	},
	{
		S( -10,   20), S( -12,   22), S( -13,   29), S( -14,   25), S(   5,   17), S(  31,   10), S(  23,   14), S(  37,    9),
		S( -16,   14), S(  -8,   20), S(   4,   21), S(  21,    9), S(  13,    9), S(  38,    1), S(  45,   -2), S(  53,   -7),
		S( -22,   11), S(   8,    6), S(   1,    8), S(   6,    3), S(  31,   -7), S(  51,  -16), S(  83,  -17), S(  53,  -19),
		S( -22,   13), S(  -8,    6), S(  -8,   12), S(  -5,    6), S(   1,   -5), S(  15,  -11), S(  28,  -10), S(  18,  -12),
		S( -31,    7), S( -31,    9), S( -23,    7), S( -17,    5), S( -14,    2), S( -16,    0), S(  12,  -10), S(  -4,  -11),
		S( -34,    5), S( -29,    0), S( -25,   -1), S( -21,    0), S( -12,   -5), S(  -4,  -13), S(  27,  -28), S(   4,  -24),
		S( -33,   -2), S( -28,   -1), S( -17,   -2), S( -15,   -4), S(  -9,  -11), S(  -1,  -15), S(  12,  -23), S( -20,  -14),
		S( -16,    3), S( -15,   -1), S( -11,    3), S(  -3,   -4), S(   4,  -10), S(   1,   -4), S(   2,  -10), S( -13,   -8),
	},
	{
		S( -20,   -7), S( -36,   16), S( -16,   34), S(  10,   26), S(   9,   25), S(  26,   14), S(  72,  -42), S(  21,   -8),
		S(  -8,  -21), S( -30,    3), S( -30,   38), S( -41,   60), S( -32,   74), S(   8,   27), S(   2,   12), S(  49,    5),
		S(  -2,  -17), S(  -8,  -10), S( -11,   22), S(  -4,   32), S(   7,   42), S(  52,   22), S(  63,  -11), S(  62,    1),
		S( -15,   -6), S( -11,    1), S( -13,   10), S( -15,   29), S( -13,   44), S(   2,   34), S(  14,   34), S(  20,   19),
		S(  -9,  -12), S( -18,   10), S( -15,    9), S( -11,   23), S(  -9,   23), S(  -8,   21), S(   4,   17), S(  11,   17),
		S( -11,  -24), S(  -8,   -9), S( -13,    4), S( -12,    2), S( -10,    7), S(  -3,    2), S(  10,  -11), S(  10,  -13),
		S(  -5,  -26), S(  -9,  -26), S(  -2,  -29), S(   0,  -23), S(  -1,  -19), S(   6,  -41), S(  13,  -63), S(  25,  -75),
		S( -13,  -27), S(  -9,  -30), S(  -6,  -28), S(   0,  -16), S(  -1,  -32), S( -13,  -30), S(  -3,  -46), S(   6,  -56),
	},
	{
		S(  24,  -54), S(  65,  -31), S( 124,  -37), S(  44,  -12), S(  56,  -32), S(  45,   -8), S(  66,    6), S( 130,  -74),
		S(-117,   31), S(  -8,   29), S(  14,   15), S( 145,  -19), S(  89,  -10), S(  34,   29), S(  31,   43), S( -42,   45),
		S(-140,   30), S(  23,   22), S(  23,   12), S(  44,    2), S(  76,   -3), S(  93,   13), S(   5,   41), S( -67,   36),
		S( -88,    8), S( -54,   18), S( -25,   14), S( -20,    4), S( -29,   -1), S( -43,   20), S( -86,   32), S(-146,   31),
		S( -87,   -2), S( -56,   10), S( -53,   13), S( -47,    8), S( -58,    7), S( -66,   14), S( -92,   23), S(-151,   24),
		S( -40,   -4), S(  -4,    4), S( -37,   10), S( -22,    5), S( -27,    5), S( -53,   16), S( -33,   12), S( -62,   13),
		S(  55,  -19), S(  32,   -3), S(  25,   -4), S(  15,   -9), S(   4,   -3), S(  11,    1), S(  34,   -2), S(  38,  -11),
		S(  51,  -48), S(  80,  -36), S(  63,  -26), S(   6,  -23), S(  51,  -39), S(   1,  -10), S(  64,  -27), S(  63,  -51),
	},
};

constexpr InitialParam MOBILITY[4][28] = {
	{S(-118,  -95), S( -31,  -52), S(  -9,  -24), S(   0,   -2), S(   9,    9), S(  11,   20), S(  19,   24), S(  28,   27), S(  36,   25), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)},
	{S( -29, -155), S( -46,  -84), S( -17,  -40), S(  -7,  -17), S(   6,   -6), S(  13,    3), S(  20,   14), S(  27,   19), S(  31,   26), S(  38,   27), S(  41,   32), S(  55,   25), S(  65,   28), S(  80,   17), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)},
	{S( -50,  -88), S( -35,  -87), S( -21,  -36), S( -13,  -20), S(  -8,  -11), S(  -4,   -2), S(  -2,    3), S(   0,    9), S(   4,   11), S(   9,   13), S(  13,   17), S(  17,   21), S(  24,   22), S(  30,   23), S(  30,   22), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)},
	{S(   0,    0), S(   0,    0), S(-102,  -21), S( -35, -196), S( -46, -110), S( -14,  -92), S( -10,  -66), S(  -7,  -44), S(  -3,  -27), S(  -1,   -1), S(   3,    5), S(   7,   11), S(  10,   20), S(  15,   20), S(  17,   24), S(  19,   33), S(  21,   36), S(  21,   45), S(  22,   51), S(  24,   54), S(  29,   59), S(  39,   49), S(  52,   51), S(  66,   42), S(  70,   48), S( 177,   -4), S(  79,   40), S(  55,   39)}
};

constexpr InitialParam THREATS[6][6] = {
	{S(  18,   -9), S(  42,    5), S(  44,   35), S(  61,  -16), S(  44,  -40), S(   0,    0)},
	{S(  -6,    6), S(   0,    0), S(  21,   21), S(  46,  -13), S(  22,  -42), S(   0,    0)},
	{S(   4,   14), S(  21,   21), S(   0,    0), S(  30,    1), S(  35,   62), S(   0,    0)},
	{S(  -7,   12), S(   3,   14), S(  12,   10), S(   0,    0), S(  42,   -6), S(   0,    0)},
	{S(  -2,    9), S(   0,    7), S(  -2,   23), S(   2,   -3), S(   0,    0), S(   0,    0)},
	{S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)}
};

constexpr InitialParam PASSED_PAWN[8] = {S(   0,    0), S(  -2,    4), S(  -7,   13), S(  -8,   36), S(  11,   61), S(   4,  120), S(  38,  106), S(   0,    0)};
constexpr InitialParam ISOLATED_PAWN[8] = {S(  -1,    5), S(  -3,  -11), S( -11,   -5), S(  -8,  -12), S( -11,  -12), S(  -6,   -3), S(  -2,  -11), S(  -7,    9)};
constexpr InitialParam PAWN_PHALANX[8] = {S(   0,    0), S(   4,   -2), S(  13,    6), S(  23,   16), S(  49,   56), S( 121,  186), S( -97,  403), S(   0,    0)};
constexpr InitialParam DEFENDED_PAWN[8] = {S(   0,    0), S(   0,    0), S(  17,   10), S(  12,    8), S(  13,   15), S(  26,   36), S( 150,   27), S(   0,    0)};

constexpr InitialParam PAWN_STORM[3][8] = {
	{S(  25,  -36), S(  19,  -18), S(  19,   -4), S(  10,    2), S(   5,    3), S(   1,    7), S(   1,    5), S(   7,  -12)},
	{S(   0,    0), S( -49,  -33), S(  29,    1), S(   2,    4), S(  -4,   11), S(  -5,   15), S(  -8,   16), S(   7,   -4)},
	{S(  -4,   -0), S(  -1,    7), S(   8,   10), S(   4,   11), S(   6,    9), S(   6,    9), S(   3,    8), S( -13,   -2)}
};
constexpr InitialParam PAWN_SHIELD[3][8] = {
	{S(   0,  -16), S( -10,  -10), S(  -7,  -13), S(  -2,  -10), S(  11,  -10), S(  13,  -18), S(  21,  -19), S(  12,   -2)},
	{S(   0,    0), S( -18,   -1), S( -12,   -6), S(   5,   -1), S(  18,   -3), S(  28,  -15), S(  52,  -25), S(  20,    3)},
	{S(  -2,  -11), S(  -2,   -8), S(   0,   -7), S(   1,   -1), S(   6,    2), S(   4,    6), S(  14,   11), S(  -6,    7)}
};
constexpr InitialParam SAFE_KNIGHT_CHECK = S(   0,    0);
constexpr InitialParam SAFE_BISHOP_CHECK = S(   0,    0);
constexpr InitialParam SAFE_ROOK_CHECK = S(   0,    0);
constexpr InitialParam SAFE_QUEEN_CHECK = S(   0,    0);

constexpr InitialParam BISHOP_PAIR = S(  20,   60);
constexpr InitialParam ROOK_OPEN[2] = {S(  26,   10), S(  15,    7)};

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
    addEvalParamArray2D(params, MOBILITY);

    addEvalParamArray2D(params, THREATS);

    addEvalParamArray(params, PASSED_PAWN);
    addEvalParamArray(params, ISOLATED_PAWN);
    addEvalParamArray(params, PAWN_PHALANX);
    addEvalParamArray(params, DEFENDED_PAWN);

    addEvalParamArray2D(params, PAWN_STORM);
    addEvalParamArray2D(params, PAWN_SHIELD);
    addEvalParam(params, SAFE_KNIGHT_CHECK);
    addEvalParam(params, SAFE_BISHOP_CHECK);
    addEvalParam(params, SAFE_ROOK_CHECK);
    addEvalParam(params, SAFE_QUEEN_CHECK);

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
    state.ss << "constexpr InitialParam PSQT[6][64] = {\n";
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
    state.ss << "constexpr InitialParam MATERIAL[6] = {";
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
void printRestParams(PrintState& state)
{
    state.ss << "constexpr InitialParam MOBILITY[4][28] = ";
    printArray2D<ALIGN_SIZE>(state, 4, 28);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam THREATS[6][6] = ";
    printArray2D<ALIGN_SIZE>(state, 6, 6);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam PASSED_PAWN[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam ISOLATED_PAWN[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam PAWN_PHALANX[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam DEFENDED_PAWN[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam PAWN_STORM[3][8] = ";
    printArray2D<ALIGN_SIZE>(state, 3, 8);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam PAWN_SHIELD[3][8] = ";
    printArray2D<ALIGN_SIZE>(state, 3, 8);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam SAFE_KNIGHT_CHECK = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam SAFE_BISHOP_CHECK = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam SAFE_ROOK_CHECK = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam SAFE_QUEEN_CHECK = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam BISHOP_PAIR = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam ROOK_OPEN[2] = ";
    printArray<ALIGN_SIZE>(state, 2);
    state.ss << ";\n";
}

void EvalFn::printEvalParams(const EvalParams& params, std::ostream& os)
{
    PrintState state{params, 0};
    printPSQTs<0>(state);
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
    printRestParams<4>(state);
    os << state.ss.str() << std::endl;
}
