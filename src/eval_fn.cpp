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
    TraceElem kingAttackerWeight[4];
    TraceElem kingAttacks[14];

    TraceElem knightOutpost;
    TraceElem bishopPair;
    TraceElem openRook[2];
};

struct EvalData
{
    ColorArray<Bitboard> mobilityArea;
    ColorArray<Bitboard> attacked;
    ColorArray<Bitboard> attackedBy2;
    ColorArray<PieceTypeArray<Bitboard>> attackedBy;
    ColorArray<Bitboard> pawnAttackSpans;
    ColorArray<Bitboard> kingRing;
    ColorArray<int> attackCount;
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

    Bitboard outpostSquares = RANK_4 | RANK_5 | (us == Color::WHITE ? RANK_6 : RANK_3);

    while (pieces)
    {
        uint32_t sq = pieces.poplsb();
        Bitboard attacks = attacks::pieceAttacks<piece>(sq, board.getAllPieces());
        evalData.attackedBy[us][piece] |= attacks;
        evalData.attackedBy2[us] |= evalData.attacked[us] & attacks;
        evalData.attacked[us] |= attacks;

        TRACE_INC(mobility[static_cast<int>(piece) - static_cast<int>(PieceType::KNIGHT)][(attacks & evalData.mobilityArea[us]).popcount()]);

        Bitboard threats = attacks & board.getColor(them);
        while (threats)
            TRACE_INC(threats[static_cast<int>(piece)][static_cast<int>(getPieceType(board.getPieceAt(threats.poplsb())))]);
        if (Bitboard kingRingAtks = evalData.kingRing[them] & attacks; kingRingAtks.any())
        {
            TRACE_INC(kingAttackerWeight[static_cast<int>(piece) - static_cast<int>(PieceType::KNIGHT)]);
            evalData.attackCount[us] += kingRingAtks.popcount();
        }

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

        if constexpr (piece == PieceType::KNIGHT)
        {
            Bitboard outposts = outpostSquares & ~evalData.pawnAttackSpans[them] & evalData.attackedBy[us][PieceType::PAWN];
            if (Bitboard::fromSquare(sq) & outposts)
                TRACE_INC(knightOutpost);
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
void evalKingPawnFile(uint32_t file, Bitboard ourPawns, Bitboard theirPawns, uint32_t theirKing, Trace& trace)
{
    uint32_t kingFile = fileOf(theirKing);
    {
        Bitboard filePawns = ourPawns & Bitboard::fileBB(file);
        // 4 = e file
        int idx = (kingFile == file) ? 1 : (kingFile >= 4) == (kingFile < file) ? 0 : 2;

        int rankDist = filePawns ?
            std::abs(rankOf(us == Color::WHITE ? filePawns.msb() : filePawns.lsb()) - rankOf(theirKing)) :
            7;
        TRACE_INC(pawnStorm[idx][rankDist]);
    }
    {
        Bitboard filePawns = theirPawns & Bitboard::fileBB(file);
        // 4 = e file
        int idx = (kingFile == file) ? 1 : (kingFile >= 4) == (kingFile < file) ? 0 : 2;
        int rankDist = filePawns ?
            std::abs(rankOf(us == Color::WHITE ? filePawns.msb() : filePawns.lsb()) - rankOf(theirKing)) :
            7;
        TRACE_INC(pawnShield[idx][rankDist]);
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

    Bitboard safe = ~evalData.attacked[them] | (~evalData.attackedBy2[them] & evalData.attackedBy[them][PieceType::KING]);

    TRACE_ADD(safeKnightCheck, (knightChecks & safe).popcount());
    TRACE_ADD(safeBishopCheck, (bishopChecks & safe).popcount());
    TRACE_ADD(safeRookCheck, (rookChecks & safe).popcount());
    TRACE_ADD(safeQueenCheck, (queenChecks & safe).popcount());
    int attackCount = std::min(evalData.attackCount[us], 13);
    TRACE_INC(kingAttacks[attackCount]);
}

void initEvalData(const Board& board, EvalData& evalData)
{
    Bitboard whitePawns = board.getPieces(Color::WHITE, PieceType::PAWN);
    Bitboard blackPawns = board.getPieces(Color::BLACK, PieceType::PAWN);
    Bitboard whitePawnAttacks = attacks::pawnAttacks<Color::WHITE>(whitePawns);
    Bitboard blackPawnAttacks = attacks::pawnAttacks<Color::BLACK>(blackPawns);
    uint32_t whiteKing = board.getPieces(Color::WHITE, PieceType::KING).lsb();
    uint32_t blackKing = board.getPieces(Color::BLACK, PieceType::KING).lsb();

    evalData.mobilityArea[Color::WHITE] = ~blackPawnAttacks;
    evalData.pawnAttackSpans[Color::WHITE] = attacks::fillUp<Color::WHITE>(whitePawnAttacks);
    evalData.attacked[Color::WHITE] = evalData.attackedBy[Color::WHITE][PieceType::PAWN] = whitePawnAttacks;

    Bitboard whiteKingAtks = attacks::kingAttacks(whiteKing);
    evalData.attackedBy[Color::WHITE][PieceType::KING] = whiteKingAtks;
    evalData.attackedBy2[Color::WHITE] = evalData.attacked[Color::WHITE] & whiteKingAtks;
    evalData.attacked[Color::WHITE] |= whiteKingAtks;
    evalData.kingRing[Color::WHITE] = (whiteKingAtks | whiteKingAtks.north()) & ~Bitboard::fromSquare(whiteKing);

    evalData.mobilityArea[Color::BLACK] = ~whitePawnAttacks;
    evalData.pawnAttackSpans[Color::BLACK] = attacks::fillUp<Color::BLACK>(blackPawnAttacks);
    evalData.attacked[Color::BLACK] = evalData.attackedBy[Color::BLACK][PieceType::PAWN] = blackPawnAttacks;

    Bitboard blackKingAtks = attacks::kingAttacks(blackKing);
    evalData.attackedBy[Color::BLACK][PieceType::KING] = blackKingAtks;
    evalData.attackedBy2[Color::BLACK] = evalData.attacked[Color::BLACK] & blackKingAtks;
    evalData.attacked[Color::BLACK] |= blackKingAtks;
    evalData.kingRing[Color::BLACK] = (blackKingAtks | blackKingAtks.south()) & ~Bitboard::fromSquare(blackKing);
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
    addCoefficientArray(trace.kingAttackerWeight);
    addCoefficientArray(trace.kingAttacks);

    addCoefficient(trace.knightOutpost);
    addCoefficient(trace.bishopPair);
    addCoefficientArray(trace.openRook);
    return {pos, m_Coefficients.size()};
}

using InitialParam = std::array<int, 2>;

#define S(mg, eg) {mg, eg}


constexpr InitialParam MATERIAL[6] = {S(  50,   78), S( 275,  341), S( 281,  357), S( 388,  628), S( 823, 1181), S(0, 0)};

constexpr InitialParam PSQT[6][64] = {
	{
		S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
		S(  37,  103), S(  48,   98), S(  32,   99), S(  65,   51), S(  42,   54), S(  33,   60), S( -32,  109), S( -30,  104),
		S(   9,   50), S(   5,   63), S(  29,   18), S(  36,  -17), S(  44,  -20), S(  74,   -6), S(  37,   44), S(  21,   34),
		S(  -9,   25), S(  -3,   26), S(   2,    3), S(   2,  -10), S(  22,  -10), S(  24,  -11), S(   9,   19), S(   9,    1),
		S( -16,    4), S(  -9,   17), S(  -4,   -4), S(   4,   -8), S(   7,   -7), S(  12,  -11), S(   1,   10), S(  -2,  -13),
		S( -23,   -4), S( -15,    6), S( -11,   -7), S(  -8,   -4), S(   1,   -2), S(  -3,   -7), S(   5,    0), S(  -6,  -19),
		S( -14,    2), S(  -5,   14), S(  -2,    1), S(  -2,   -2), S(   5,    9), S(  29,   -6), S(  22,    4), S(  -1,  -16),
		S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
	},
	{
		S(-103,  -28), S( -97,   -5), S( -64,    8), S( -22,   -5), S(   3,    0), S( -48,  -19), S( -83,    0), S( -74,  -46),
		S( -17,    2), S(  -2,    7), S(   4,    3), S(  19,    3), S(  14,   -4), S(  33,  -14), S(  14,   -1), S(   6,  -12),
		S(   1,   -4), S(   8,    0), S(  18,    4), S(  23,    5), S(  36,   -2), S(  61,  -22), S(  18,   -9), S(  22,  -13),
		S(   5,    9), S(  10,    3), S(  26,    6), S(  38,    9), S(  34,    8), S(  40,    2), S(  25,    1), S(  33,   -2),
		S(  -1,    8), S(   9,   -1), S(  15,   12), S(  24,   11), S(  16,   21), S(  20,    4), S(  15,    2), S(   3,   10),
		S( -11,    2), S(  -1,    1), S(   5,    2), S(   7,   15), S(  19,   13), S(   7,   -3), S(  17,   -4), S(   4,    4),
		S( -15,    5), S( -12,    9), S(  -4,    0), S(  10,    2), S(   8,    1), S(   8,   -3), S(   9,   -2), S(   6,   19),
		S( -46,   16), S(  -7,   -1), S( -22,   -1), S( -10,    1), S(  -6,    5), S(   2,   -7), S(  -4,    6), S( -18,   14),
	},
	{
		S( -16,    4), S( -48,   10), S( -39,    3), S( -81,   13), S( -69,   10), S( -61,    2), S( -27,    1), S( -55,   -1),
		S( -14,   -8), S( -12,   -2), S( -12,   -3), S( -18,    0), S( -11,   -7), S( -15,   -6), S( -42,    6), S( -27,   -2),
		S(   6,    7), S(  15,   -1), S(   7,    1), S(  15,   -6), S(   8,   -2), S(  34,    2), S(  23,   -1), S(  21,    9),
		S(  -8,    5), S(   3,    8), S(   5,    3), S(  20,   11), S(   6,    5), S(   8,    8), S(  -4,    8), S(  -4,    4),
		S(   1,    3), S(  -8,    8), S(   4,    6), S(  11,    7), S(  10,    3), S(   0,    2), S(  -1,    4), S(   5,   -3),
		S(   3,    4), S(  16,    2), S(   8,    1), S(  11,    3), S(  12,    8), S(  12,   -1), S(  15,   -4), S(  20,   -4),
		S(  25,    7), S(  13,  -11), S(  21,  -13), S(   5,   -2), S(  14,   -1), S(  21,   -9), S(  33,  -11), S(  25,   -8),
		S(  14,   -4), S(  25,    5), S(  11,   -2), S(   2,   -1), S(  10,   -5), S(   6,    7), S(  22,  -11), S(  33,  -17),
	},
	{
		S( -12,   18), S( -10,   19), S( -15,   28), S( -22,   27), S( -11,   20), S(  11,   14), S(  15,   15), S(  26,    9),
		S( -10,   13), S(  -3,   20), S(  11,   21), S(  27,   10), S(  13,   11), S(  23,    6), S(  31,    2), S(  34,   -2),
		S( -16,   11), S(  15,    6), S(   6,   10), S(   8,    5), S(  27,   -3), S(  38,  -11), S(  62,  -11), S(  22,  -10),
		S( -15,   12), S(   2,    5), S(   0,   11), S(   3,    5), S(   8,   -6), S(  15,   -9), S(  15,   -6), S(   7,   -9),
		S( -23,    5), S( -22,    6), S( -15,    5), S(  -7,    2), S(  -6,    0), S( -14,    1), S(   4,   -7), S( -10,   -9),
		S( -25,    1), S( -21,   -3), S( -17,   -5), S( -12,   -3), S(  -5,   -7), S(  -3,  -12), S(  17,  -24), S(   3,  -24),
		S( -26,   -6), S( -20,   -5), S( -10,   -6), S(  -8,   -7), S(  -4,  -13), S(   0,  -16), S(   7,  -22), S( -19,  -15),
		S(  -9,    0), S(  -8,   -5), S(  -4,    0), S(   3,   -6), S(   9,  -12), S(   5,   -7), S(   1,  -10), S(  -9,   -9),
	},
	{
		S( -22,    0), S( -32,    9), S( -21,   32), S(  -5,   26), S( -11,   28), S(  -2,   19), S(  53,  -35), S(  -3,    4),
		S(  -6,  -15), S( -21,   -1), S( -22,   35), S( -33,   53), S( -38,   69), S(  -3,   30), S(  -6,   11), S(  31,   11),
		S(   3,  -12), S(  -2,   -3), S(  -8,   24), S(  -6,   31), S(  -1,   40), S(   7,   13), S(  25,   -3), S(  19,    2),
		S(  -9,    2), S(  -3,   11), S(  -5,   15), S(  -9,   29), S( -16,   31), S(   0,   20), S(  -1,   31), S(   6,   11),
		S(   2,   -6), S(  -8,   17), S(  -5,   19), S(  -6,   23), S(  -1,   24), S(  -1,   17), S(   6,    9), S(  11,    3),
		S(   0,  -18), S(   3,   -2), S(  -5,    9), S(  -2,   12), S(   1,   15), S(   5,    3), S(  14,  -15), S(  14,  -24),
		S(   6,  -27), S(   1,  -28), S(   9,  -22), S(  11,  -17), S(  10,  -13), S(  16,  -40), S(  21,  -70), S(  34,  -87),
		S(  -2,  -32), S(   2,  -29), S(   5,  -24), S(  10,  -11), S(   9,  -27), S(  -3,  -36), S(   5,  -51), S(  13,  -52),
	},
	{
		S(  39,  -61), S(  48,  -32), S( 104,  -36), S(  31,  -11), S(  45,  -30), S(  -4,   -3), S(  27,    7), S( 120,  -77),
		S(-115,   27), S(   7,   25), S(  22,   14), S( 144,  -18), S(  89,   -9), S(  38,   27), S(  21,   41), S( -64,   44),
		S(-119,   24), S(  40,   18), S(  37,   10), S(  52,    2), S(  91,   -4), S( 102,   11), S(   4,   40), S( -58,   32),
		S( -71,    3), S( -28,   14), S(  -3,   11), S(   0,    3), S(  -8,   -3), S( -23,   17), S( -67,   28), S(-146,   29),
		S( -76,   -6), S( -31,    6), S( -22,    8), S( -19,    5), S( -27,    4), S( -34,   10), S( -66,   18), S(-147,   21),
		S( -41,   -7), S(   9,    2), S( -17,    8), S(   1,    2), S(   0,    2), S( -20,   11), S( -11,    8), S( -67,   12),
		S(  35,  -19), S(  25,   -2), S(  19,   -1), S(   2,   -5), S(  -2,   -1), S(   5,    3), S(  25,   -1), S(  19,  -11),
		S(  16,  -43), S(  46,  -29), S(  21,  -15), S( -35,  -13), S(   4,  -25), S( -33,    0), S(  26,  -17), S(  24,  -43),
	},
};

constexpr InitialParam MOBILITY[4][28] = {
	{S(-124, -104), S( -34,  -56), S( -13,  -28), S(  -4,   -5), S(   5,    6), S(   8,   17), S(  16,   22), S(  26,   25), S(  35,   24), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)},
	{S( -23, -161), S( -43,  -85), S( -15,  -40), S(  -6,  -18), S(   6,   -6), S(  13,    3), S(  19,   14), S(  24,   20), S(  27,   26), S(  32,   29), S(  33,   34), S(  44,   28), S(  44,   33), S(  54,   25), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)},
	{S( -48,  -98), S( -30,  -87), S( -18,  -33), S( -10,  -17), S(  -5,   -9), S(  -2,   -2), S(   0,    3), S(   2,    8), S(   5,   10), S(   9,   12), S(  11,   16), S(  13,   20), S(  16,   22), S(  19,   23), S(  19,   22), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)},
	{S(   0,    0), S(   0,    0), S(-150,  -34), S( -24, -230), S( -39,  -96), S(  -7,  -88), S(  -2,  -62), S(   0,  -41), S(   4,  -26), S(   6,   -4), S(  10,   -0), S(  14,    4), S(  17,   11), S(  22,    9), S(  24,   11), S(  26,   16), S(  27,   17), S(  27,   22), S(  29,   24), S(  31,   23), S(  37,   21), S(  49,    4), S(  63,   -2), S(  78,  -17), S(  79,  -15), S( 188,  -78), S(  87,  -37), S(  38,  -32)}
};

constexpr InitialParam THREATS[6][6] = {
	{S(  18,   -8), S(  40,    1), S(  41,   35), S(  68,  -18), S(  45,  -45), S(   0,    0)},
	{S(  -5,    8), S(   0,    0), S(  21,   21), S(  51,  -13), S(  24,  -45), S(   0,    0)},
	{S(   2,   13), S(  20,   21), S(   0,    0), S(  31,   -1), S(  36,   56), S(   0,    0)},
	{S(  -9,   13), S(   2,   18), S(  10,   12), S(   0,    0), S(  45,  -12), S(   0,    0)},
	{S(  -2,    6), S(   1,   10), S(  -2,   24), S(   2,   -4), S(   0,    0), S(   0,    0)},
	{S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)}
};

constexpr InitialParam PASSED_PAWN[8] = {S(   0,    0), S(  -2,    4), S(  -7,   13), S(  -8,   37), S(  12,   61), S(   7,  119), S(  48,  109), S(   0,    0)};
constexpr InitialParam ISOLATED_PAWN[8] = {S(   0,    4), S(  -3,  -10), S( -11,   -5), S(  -7,  -12), S( -11,  -13), S(  -7,   -3), S(  -1,  -11), S( -10,    9)};
constexpr InitialParam PAWN_PHALANX[8] = {S(   0,    0), S(   5,   -2), S(  12,    6), S(  22,   17), S(  49,   57), S( 132,  184), S(-145,  418), S(   0,    0)};
constexpr InitialParam DEFENDED_PAWN[8] = {S(   0,    0), S(   0,    0), S(  17,   10), S(  12,    8), S(  13,   15), S(  26,   36), S( 160,   27), S(   0,    0)};

constexpr InitialParam PAWN_STORM[3][8] = {
	{S(  25,  -36), S(  24,  -19), S(  20,   -4), S(  11,    2), S(   4,    4), S(  -1,    7), S(   1,    5), S(  10,  -12)},
	{S(   0,    0), S( -50,  -31), S(  28,    0), S(   4,    4), S(  -5,   10), S(  -7,   14), S(  -8,   15), S(   8,   -5)},
	{S(   1,   -1), S(   1,    6), S(   8,    9), S(   4,   11), S(   4,    9), S(   3,    9), S(   3,    8), S(  -8,   -2)}
};
constexpr InitialParam PAWN_SHIELD[3][8] = {
	{S(   2,  -15), S(  -9,  -10), S(  -6,  -13), S(  -2,  -10), S(  10,   -9), S(  13,  -18), S(  22,  -19), S(   9,   -1)},
	{S(   0,    0), S( -17,   -1), S( -11,   -6), S(   6,   -1), S(  16,   -2), S(  26,  -14), S(  52,  -26), S(  20,    3)},
	{S(  -2,  -11), S(  -2,   -8), S(   0,   -7), S(   1,   -0), S(   4,    3), S(   3,    6), S(  14,   11), S(  -5,    7)}
};
constexpr InitialParam SAFE_KNIGHT_CHECK = S(  73,   -3);
constexpr InitialParam SAFE_BISHOP_CHECK = S(  15,   -6);
constexpr InitialParam SAFE_ROOK_CHECK = S(  50,   -1);
constexpr InitialParam SAFE_QUEEN_CHECK = S(  29,   13);
constexpr InitialParam KING_ATTACKER_WEIGHT[4] = {S(  15,   -4), S(  10,   -1), S(  10,  -17), S(   1,   13)};
constexpr InitialParam KING_ATTACKS[32] = {S( -24,    9), S( -29,    7), S( -32,    5), S( -30,   11), S( -21,    8), S(  -4,    4), S(  23,   -7), S(  57,  -21), S( 102,  -41), S( 139,  -57), S( 187,  -71), S( 260, -113), S( 265,  -95), S( 257,  -86)};

constexpr InitialParam KNIGHT_OUTPOST = S(  33,   22);
constexpr InitialParam BISHOP_PAIR = S(  21,   58);
constexpr InitialParam ROOK_OPEN[2] = {S(  27,    9), S(  15,    8)};

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
    addEvalParamArray(params, KING_ATTACKER_WEIGHT);
    addEvalParamArray(params, KING_ATTACKS);

    addEvalParam(params, KNIGHT_OUTPOST);
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

    state.ss << "constexpr InitialParam KING_ATTACKER_WEIGHT[4] = ";
    printArray<ALIGN_SIZE>(state, 4);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam KING_ATTACKS[14] = ";
    printArray<ALIGN_SIZE>(state, 14);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam KNIGHT_OUTPOST = ";
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
