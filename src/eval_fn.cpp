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
    TraceElem ourPasserProximity[8];
    TraceElem theirPasserProximity[8];

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

    TraceElem tempo;
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

    Bitboard passedPawns;
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

    Bitboard occupancy = board.getAllPieces();
    if constexpr (piece == PieceType::BISHOP)
        occupancy ^= board.getPieces(us, PieceType::BISHOP) | board.getPieces(us, PieceType::QUEEN);
    else if constexpr (piece == PieceType::ROOK)
        occupancy ^= board.getPieces(us, PieceType::ROOK) | board.getPieces(us, PieceType::QUEEN);
    else if constexpr (piece == PieceType::QUEEN)
        occupancy ^= board.getPieces(us, PieceType::BISHOP) | board.getPieces(us, PieceType::ROOK);

    Bitboard outpostSquares = RANK_4 | RANK_5 | (us == Color::WHITE ? RANK_6 : RANK_3);

    while (pieces)
    {
        uint32_t sq = pieces.poplsb();
        Bitboard attacks = attacks::pieceAttacks<piece>(sq, occupancy);
        if (board.checkBlockers(us) & Bitboard::fromSquare(sq))
            attacks &= attacks::inBetweenSquares(sq, board.getPieces(us, PieceType::KING).lsb());

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
void evaluatePawns(const Board& board, EvalData& evalData, Trace& trace)
{
    Bitboard ourPawns = board.getPieces(us, PieceType::PAWN);

    Bitboard pawns = ourPawns;
    while (pawns)
    {
        uint32_t sq = pawns.poplsb();
        if (board.isPassedPawn(sq))
        {
            evalData.passedPawns |= Bitboard::fromSquare(sq);
            TRACE_INC(passedPawn[relativeRankOf<us>(sq)]);
        }
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

template<Color us>
void evaluateKingPawn(const Board & board, const EvalData & evalData, Trace& trace)
{
    constexpr Color them = ~us;
    uint32_t ourKing = board.getPieces(us, PieceType::KING).lsb();
    uint32_t theirKing = board.getPieces(them, PieceType::KING).lsb();

    Bitboard passers = evalData.passedPawns & board.getColor(us);

    while (passers.any())
    {
        uint32_t passer = passers.poplsb();
        TRACE_INC(ourPasserProximity[chebyshev(ourKing, passer)]);
        TRACE_INC(theirPasserProximity[chebyshev(theirKing, passer)]);
    }
}

void evaluatePawns(const Board& board, EvalData& evalData, Trace& trace)
{
    evaluatePawns<Color::WHITE>(board, evalData, trace);
    evaluatePawns<Color::BLACK>(board, evalData, trace);
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

    evaluatePawns(board, evalData, trace);
    evaluateKingPawn<Color::WHITE>(board, evalData, trace);
    evaluateKingPawn<Color::BLACK>(board, evalData, trace);
    evaluateThreats<Color::WHITE>(board, evalData, trace);
    evaluateThreats<Color::BLACK>(board, evalData, trace);

    trace.tempo[board.sideToMove()]++;

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
    addCoefficientArray(trace.ourPasserProximity);
    addCoefficientArray(trace.theirPasserProximity);

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

    addCoefficient(trace.tempo);
    return {pos, m_Coefficients.size()};
}

using InitialParam = std::array<int, 2>;

#define S(mg, eg) {mg, eg}

constexpr InitialParam MATERIAL[6] = {S(  58,   85), S( 280,  352), S( 286,  370), S( 398,  652), S( 840, 1198), S(0, 0)};

constexpr InitialParam PSQT[6][64] = {
	{
		S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
		S(  35,   76), S(  43,   78), S(  27,   85), S(  63,   43), S(  48,   50), S(  35,   57), S( -26,   95), S( -30,   89),
		S(  15,   38), S(  10,   54), S(  37,   13), S(  45,  -16), S(  54,  -17), S(  85,   -8), S(  46,   37), S(  30,   27),
		S(  -8,   23), S(  -2,   25), S(   5,    2), S(   5,  -10), S(  25,   -9), S(  27,  -11), S(  11,   16), S(  12,    0),
		S( -15,    6), S( -11,   20), S(  -2,   -3), S(   7,   -8), S(   9,   -7), S(  13,  -11), S(   2,    9), S(   0,  -12),
		S( -25,   -1), S( -19,   10), S( -11,   -6), S(  -9,   -3), S(   1,   -2), S(  -4,   -7), S(   4,   -1), S(  -7,  -18),
		S( -17,    4), S( -10,   16), S(  -4,    1), S(  -4,   -3), S(   4,    9), S(  28,   -7), S(  20,    3), S(  -3,  -16),
		S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
	},
	{
		S( -96,  -33), S( -92,   -6), S( -69,    9), S( -24,   -6), S(   4,   -6), S( -50,  -24), S( -82,   -5), S( -66,  -52),
		S( -14,    6), S(   1,   10), S(   3,    1), S(  13,    1), S(  12,   -9), S(  29,  -14), S(  11,    2), S(   6,  -10),
		S(   3,    0), S(   6,    0), S(  13,    4), S(  19,    3), S(  31,   -3), S(  59,  -23), S(  18,  -10), S(  23,  -14),
		S(   7,   11), S(  11,    2), S(  26,    5), S(  36,    9), S(  31,    9), S(  39,    2), S(  26,    0), S(  36,   -1),
		S(   0,   10), S(  11,    0), S(  14,   11), S(  25,   11), S(  17,   21), S(  19,    4), S(  16,    2), S(   3,   11),
		S( -12,    1), S(  -1,    0), S(   2,    1), S(   8,   15), S(  21,   13), S(   4,   -5), S(  18,   -4), S(   3,    4),
		S( -15,    7), S( -12,   11), S(  -5,    0), S(   8,    2), S(   7,    1), S(   9,   -1), S(  10,   -1), S(   5,   19),
		S( -44,   22), S(  -8,   -1), S( -16,    1), S(  -7,    4), S(   0,    5), S(   4,   -6), S(  -7,    7), S( -12,   17),
	},
	{
		S( -10,    8), S( -42,   11), S( -42,    5), S( -77,   11), S( -71,   11), S( -66,    1), S( -29,    4), S( -44,   -2),
		S(  -4,   -6), S(  -9,   -1), S(  -5,   -5), S( -17,    0), S( -10,   -7), S( -17,   -3), S( -50,    8), S( -34,    3),
		S(   7,    8), S(  17,   -1), S(   6,    1), S(  18,   -7), S(   7,   -2), S(  37,    3), S(  20,    1), S(  24,    7),
		S(  -6,    5), S(  -1,    8), S(   9,    2), S(  21,   12), S(  10,    5), S(   6,    9), S(  -6,    7), S(   0,    5),
		S(   2,    4), S(  -6,    6), S(   1,    5), S(  13,    7), S(  11,    4), S(  -1,    0), S(   0,    4), S(   8,   -6),
		S(   4,    1), S(  17,    2), S(   9,    0), S(   5,    5), S(  10,    8), S(  12,   -1), S(  19,   -6), S(  22,   -6),
		S(  25,    6), S(  11,  -14), S(  19,  -13), S(   1,   -1), S(  10,    0), S(  22,   -9), S(  32,  -12), S(  28,  -10),
		S(  15,   -5), S(  23,   10), S(   9,    3), S(   5,    0), S(  16,   -5), S(   5,   10), S(  27,   -9), S(  38,  -21),
	},
	{
		S( -11,   17), S( -13,   20), S( -20,   29), S( -26,   26), S( -15,   18), S(   3,   16), S(  -1,   21), S(  19,   11),
		S(  -1,   11), S(   5,   17), S(  14,   18), S(  29,    7), S(  14,    8), S(  16,    8), S(  25,    4), S(  29,    1),
		S(  -8,   11), S(  21,    6), S(  11,    9), S(  10,    4), S(  30,   -2), S(  37,   -9), S(  52,   -7), S(  22,   -7),
		S(  -9,   13), S(   5,    7), S(   4,   11), S(   5,    7), S(  11,   -4), S(  13,   -6), S(   8,    0), S(   7,   -3),
		S( -16,    7), S( -14,    7), S(  -8,    5), S(  -2,    3), S(  -1,    1), S( -14,    3), S(   3,   -5), S( -12,   -3),
		S( -20,    2), S( -16,   -2), S( -12,   -4), S( -10,   -2), S(   0,   -7), S(  -4,  -10), S(  16,  -23), S(   1,  -20),
		S( -21,   -4), S( -16,   -4), S(  -7,   -5), S(  -6,   -6), S(  -2,  -12), S(   2,  -15), S(   8,  -20), S( -17,  -15),
		S( -13,   -3), S( -12,   -8), S( -11,   -2), S(  -4,  -10), S(   2,  -16), S(  -3,  -11), S(  -4,  -12), S( -16,  -11),
	},
	{
		S( -16,  -11), S( -34,    3), S( -27,   26), S(  -2,   15), S(  -8,   17), S(   3,   11), S(  47,  -37), S(  13,  -11),
		S(   7,  -14), S(  -7,   -8), S(  -8,   22), S( -20,   40), S( -24,   56), S(   4,   23), S(   8,    3), S(  40,    9),
		S(   9,   -5), S(   6,   -4), S(  -2,   21), S(   0,   25), S( -12,   41), S(  11,   10), S(  27,   -4), S(  28,    0),
		S(  -1,    8), S(  -4,   18), S(   0,   15), S( -11,   29), S( -17,   30), S(   1,   17), S(   1,   32), S(  11,   13),
		S(  -1,    3), S(  -3,   18), S(  -6,   14), S(  -9,   24), S(  -2,   23), S(  -4,   20), S(   9,    7), S(  14,    7),
		S(   4,  -17), S(   2,   -4), S(  -9,    8), S(  -8,   15), S(  -3,   20), S(   0,    6), S(  13,  -14), S(  16,  -20),
		S(   5,  -31), S(  -1,  -30), S(   3,  -20), S(   3,   -5), S(   2,   -5), S(   8,  -32), S(  17,  -64), S(  30,  -76),
		S( -14,  -28), S( -14,  -24), S( -12,  -13), S(  -9,  -13), S(  -8,  -15), S( -18,  -25), S(  -7,  -39), S(  10,  -47),
	},
	{
		S(  47,  -54), S(  87,  -30), S( 123,  -33), S(  57,  -11), S(  65,  -34), S( -19,    2), S(  18,    7), S( 102,  -70),
		S( -97,   28), S(  12,   28), S(  33,   13), S( 150,  -17), S(  92,  -10), S(  45,   27), S(   9,   45), S( -90,   48),
		S(-109,   26), S(  40,   19), S(  36,    9), S(  47,    3), S(  86,    1), S( 104,   13), S(  -6,   43), S( -69,   32),
		S( -64,    2), S( -23,    9), S( -13,    7), S( -13,    2), S( -18,   -2), S( -26,   16), S( -62,   26), S(-151,   29),
		S( -69,  -10), S( -31,    0), S( -30,    4), S( -30,    2), S( -35,    2), S( -38,    6), S( -71,   15), S(-149,   19),
		S( -48,   -5), S(   8,   -2), S( -20,    3), S(  -4,    0), S(  -6,   -1), S( -23,    6), S( -14,    5), S( -71,   10),
		S(  33,  -15), S(  23,   -1), S(  19,   -1), S(   2,   -3), S(  -3,   -1), S(   7,    1), S(  23,   -1), S(  18,  -11),
		S(  20,  -34), S(  52,  -25), S(  25,  -10), S( -30,   -3), S(   7,  -19), S( -23,    3), S(  31,  -15), S(  29,  -39),
	},
};

constexpr InitialParam MOBILITY[4][28] = {
	{S(  -5,   -5), S( -27,  -51), S(  -5,  -21), S(   4,    4), S(  14,   16), S(  17,   27), S(  26,   33), S(  36,   37), S(  45,   36), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)},
	{S(   5,  -13), S( -29,  -78), S(  -8,  -31), S(  -0,   -8), S(  12,    3), S(  19,   13), S(  24,   24), S(  30,   31), S(  33,   37), S(  37,   40), S(  37,   45), S(  48,   39), S(  42,   46), S(  60,   34), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)},
	{S( -45,    6), S(   4,  -32), S( -20,  -31), S( -10,  -15), S(  -5,  -11), S(   1,    5), S(   2,   13), S(  -1,   20), S(   3,   22), S(   7,   26), S(  11,   30), S(  13,   36), S(  17,   39), S(  24,   40), S(  30,   35), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)},
	{S( -30,   33), S( -47,  -47), S( -93,  -36), S( -61, -252), S( -65,  -96), S( -23,  -36), S( -14,  -11), S(  -6,   -5), S(   2,    3), S(   5,   25), S(   8,   33), S(  13,   42), S(  16,   49), S(  20,   51), S(  23,   54), S(  26,   58), S(  29,   60), S(  29,   65), S(  29,   69), S(  31,   71), S(  38,   66), S(  45,   54), S(  50,   56), S(  75,   29), S(  79,   31), S( 123,   -4), S( 123,   -7), S(  95,  -16)}
};

constexpr InitialParam THREATS[6][6] = {
	{S(   9,  -14), S(  59,   22), S(  59,   57), S(  85,    4), S(  72,  -26), S(   0,    0)},
	{S(  -5,   12), S(  18,   31), S(  31,   35), S(  67,   14), S(  51,  -24), S(   0,    0)},
	{S(   2,   17), S(  23,   26), S( -12,   14), S(  50,   23), S(  61,   80), S(   0,    0)},
	{S(  -6,   21), S(   4,   22), S(  15,   14), S(  -1,  -22), S(  64,   18), S(   0,    0)},
	{S(  -1,   14), S(   3,   10), S(  -2,   26), S(  -2,    5), S(  -7,  -46), S( 135,   -2)},
	{S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)}
};

constexpr InitialParam PASSED_PAWN[8] = {S(   0,    0), S( -17,  -67), S( -15,  -51), S( -13,  -18), S(  12,   18), S(   1,   86), S(  49,   95), S(   0,    0)};
constexpr InitialParam ISOLATED_PAWN[8] = {S(  -4,    6), S(  -3,  -11), S( -11,   -5), S(  -6,  -12), S( -11,  -13), S(  -7,   -4), S(  -2,  -12), S( -11,    7)};
constexpr InitialParam PAWN_PHALANX[8] = {S(   0,    0), S(   4,   -2), S(  13,    6), S(  22,   17), S(  49,   57), S( 135,  179), S(-164,  436), S(   0,    0)};
constexpr InitialParam DEFENDED_PAWN[8] = {S(   0,    0), S(   0,    0), S(  19,   11), S(  12,    8), S(  12,   16), S(  28,   40), S( 170,   29), S(   0,    0)};
constexpr InitialParam OUR_PASSER_PROXIMITY[8] = {S(   0,    0), S(   5,   52), S(  -5,   39), S(  -4,   23), S(  -1,   12), S(   2,   10), S(  17,    8), S(   5,    7)};
constexpr InitialParam THEIR_PASSER_PROXIMITY[8] = {S(   0,    0), S( -72,   -7), S(   5,    1), S(  -1,   26), S(   5,   36), S(   5,   44), S(   8,   49), S( -11,   48)};

constexpr InitialParam PAWN_STORM[3][8] = {
	{S(  46,  -35), S(  26,  -17), S(  19,   -3), S(   9,   -0), S(   2,    2), S(  -2,    6), S(  -1,    5), S(   8,  -11)},
	{S(   0,    0), S(  13,  -29), S(  29,    1), S(   3,    1), S(  -5,    7), S(  -7,   12), S(  -8,   14), S(   7,   -2)},
	{S(   2,   -2), S(  -6,    5), S(   4,    9), S(   3,    9), S(   4,    8), S(   4,   10), S(   4,    9), S(  -8,   -4)}
};
constexpr InitialParam PAWN_SHIELD[3][8] = {
	{S(   1,  -13), S(  -9,   -7), S(  -5,  -11), S(  -0,  -10), S(  14,  -14), S(  18,  -26), S(  30,  -29), S(   8,    0)},
	{S(   0,    0), S( -16,    3), S(  -9,   -4), S(   9,   -4), S(  21,  -11), S(  36,  -31), S(  52,  -39), S(  17,    2)},
	{S(  -3,   -8), S(  -2,   -6), S(   0,   -5), S(   1,    0), S(   5,    1), S(   6,    1), S(  24,    5), S(  -7,    7)}
};
constexpr InitialParam SAFE_KNIGHT_CHECK = S(  79,   -5);
constexpr InitialParam SAFE_BISHOP_CHECK = S(  16,   -6);
constexpr InitialParam SAFE_ROOK_CHECK = S(  57,   -3);
constexpr InitialParam SAFE_QUEEN_CHECK = S(  32,   10);
constexpr InitialParam KING_ATTACKER_WEIGHT[4] = {S(  15,   -5), S(   9,   -1), S(   7,  -19), S(  -2,   12)};
constexpr InitialParam KING_ATTACKS[14] = {S( -23,    8), S( -26,    4), S( -29,    2), S( -28,    7), S( -20,    4), S(  -5,   -1), S(  17,   -7), S(  47,  -21), S(  93,  -43), S( 122,  -46), S( 159,  -60), S( 183,  -48), S( 219, -128), S( 185,    3)};

constexpr InitialParam KNIGHT_OUTPOST = S(  33,   23);
constexpr InitialParam BISHOP_PAIR = S(  20,   59);
constexpr InitialParam ROOK_OPEN[2] = {S(  22,    6), S(  11,    5)};

constexpr InitialParam TEMPO = S(  28,   25);

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
    addEvalParamArray(params, OUR_PASSER_PROXIMITY);
    addEvalParamArray(params, THEIR_PASSER_PROXIMITY);

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
    addEvalParam(params, TEMPO);
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

    state.ss << "constexpr InitialParam OUR_PASSER_PROXIMITY[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam THEIR_PASSER_PROXIMITY[8] = ";
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

    state.ss << "constexpr InitialParam TEMPO = ";
    printSingle<ALIGN_SIZE>(state);
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
