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

    TraceElem threatByPawn[6];
    TraceElem threatByKnight[2][6];
    TraceElem threatByBishop[2][6];
    TraceElem threatByRook[2][6];
    TraceElem threatByQueen[2][6];
    TraceElem threatByKing[6];
    TraceElem pushThreat;

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
    TraceElem bishopPawns[7];
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
    Bitboard ourPawns = board.pieces(us, PieceType::PAWN);
    Bitboard theirPawns = board.pieces(them, PieceType::PAWN);

    Bitboard pieces = board.pieces(us, piece);
    if constexpr (piece == PieceType::BISHOP)
        if (pieces.multiple())
            TRACE_INC(bishopPair);

    Bitboard occupancy = board.allPieces();
    if constexpr (piece == PieceType::BISHOP)
        occupancy ^= board.pieces(us, PieceType::BISHOP) | board.pieces(us, PieceType::QUEEN);
    else if constexpr (piece == PieceType::ROOK)
        occupancy ^= board.pieces(us, PieceType::ROOK) | board.pieces(us, PieceType::QUEEN);
    else if constexpr (piece == PieceType::QUEEN)
        occupancy ^= board.pieces(us, PieceType::BISHOP) | board.pieces(us, PieceType::ROOK);

    Bitboard outpostSquares = RANK_4 | RANK_5 | (us == Color::WHITE ? RANK_6 : RANK_3);

    while (pieces.any())
    {
        uint32_t sq = pieces.poplsb();
        Bitboard attacks = attacks::pieceAttacks<piece>(sq, occupancy);
        if ((board.checkBlockers(us) & Bitboard::fromSquare(sq)).any())
            attacks &= attacks::inBetweenSquares(sq, board.pieces(us, PieceType::KING).lsb());

        evalData.attackedBy[us][piece] |= attacks;
        evalData.attackedBy2[us] |= evalData.attacked[us] & attacks;
        evalData.attacked[us] |= attacks;

        TRACE_INC(mobility[static_cast<int>(piece) - static_cast<int>(PieceType::KNIGHT)][(attacks & evalData.mobilityArea[us]).popcount()]);

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
            if ((Bitboard::fromSquare(sq) & outposts).any())
                TRACE_INC(knightOutpost);
        }

        if constexpr (piece == PieceType::BISHOP)
        {
            bool lightSquare = (Bitboard::fromSquare(sq) & LIGHT_SQUARES).any();
            Bitboard sameColorPawns = board.pieces(us, PieceType::PAWN) & (lightSquare ? LIGHT_SQUARES : DARK_SQUARES);
            TRACE_INC(bishopPawns[std::min(sameColorPawns.popcount(), 6u)]);
        }
    }
}



template<Color us>
void evaluatePawns(const Board& board, EvalData& evalData, Trace& trace)
{
    Bitboard ourPawns = board.pieces(us, PieceType::PAWN);

    Bitboard pawns = ourPawns;
    while (pawns.any())
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
    while (phalanx.any())
        TRACE_INC(pawnPhalanx[relativeRankOf<us>(phalanx.poplsb())]);

    // shift with opposite color is intentional
    Bitboard defended = ourPawns & attacks::pawnAttacks<us>(ourPawns);
    while (defended.any())
        TRACE_INC(defendedPawn[relativeRankOf<us>(defended.poplsb())]);

}

template<Color us>
void evaluateKingPawn(const Board & board, const EvalData & evalData, Trace& trace)
{
    constexpr Color them = ~us;
    uint32_t ourKing = board.pieces(us, PieceType::KING).lsb();
    uint32_t theirKing = board.pieces(them, PieceType::KING).lsb();

    Bitboard passers = evalData.passedPawns & board.pieces(us);

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

    Bitboard defendedBB = evalData.attacked[them];

    Bitboard pawnThreats = evalData.attackedBy[us][PieceType::PAWN] & board.pieces(them);
    while (pawnThreats.any())
    {
        PieceType threatened = getPieceType(board.pieceAt(pawnThreats.poplsb()));
        TRACE_INC(threatByPawn[static_cast<int>(threatened)]);
    }

    Bitboard knightThreats = evalData.attackedBy[us][PieceType::KNIGHT] & board.pieces(them);
    while (knightThreats.any())
    {
        int threat = knightThreats.poplsb();
        PieceType threatened = getPieceType(board.pieceAt(threat));
        bool defended = (defendedBB & Bitboard::fromSquare(threat)).any();
        TRACE_INC(threatByKnight[defended][static_cast<int>(threatened)]);
    }

    Bitboard bishopThreats = evalData.attackedBy[us][PieceType::BISHOP] & board.pieces(them);
    while (bishopThreats.any())
    {
        int threat = bishopThreats.poplsb();
        PieceType threatened = getPieceType(board.pieceAt(threat));
        bool defended = (defendedBB & Bitboard::fromSquare(threat)).any();
        TRACE_INC(threatByBishop[defended][static_cast<int>(threatened)]);
    }

    Bitboard rookThreats = evalData.attackedBy[us][PieceType::ROOK] & board.pieces(them);
    while (rookThreats.any())
    {
        int threat = rookThreats.poplsb();
        PieceType threatened = getPieceType(board.pieceAt(threat));
        bool defended = (defendedBB & Bitboard::fromSquare(threat)).any();
        TRACE_INC(threatByRook[defended][static_cast<int>(threatened)]);
    }

    Bitboard queenThreats = evalData.attackedBy[us][PieceType::QUEEN] & board.pieces(them);
    while (queenThreats.any())
    {
        int threat = queenThreats.poplsb();
        PieceType threatened = getPieceType(board.pieceAt(threat));
        bool defended = (defendedBB & Bitboard::fromSquare(threat)).any();
        TRACE_INC(threatByQueen[defended][static_cast<int>(threatened)]);
    }

    Bitboard kingThreats = evalData.attackedBy[us][PieceType::KING] & board.pieces(them) & ~defendedBB;
    while (kingThreats.any())
    {
        PieceType threatened = getPieceType(board.pieceAt(kingThreats.poplsb()));
        TRACE_INC(threatByKing[static_cast<int>(threatened)]);
    }


    Bitboard nonPawnEnemies = board.pieces(them) & ~board.pieces(PieceType::PAWN);

    Bitboard safe = ~defendedBB | (evalData.attacked[us] & ~evalData.attackedBy[them][PieceType::PAWN]);
    Bitboard pushes = attacks::pawnPushes<us>(board.pieces(us, PieceType::PAWN)) & ~board.allPieces();
    pushes |= attacks::pawnPushes<us>(pushes & Bitboard::nthRank<us, 2>()) & ~board.allPieces();

    Bitboard pushThreats = attacks::pawnAttacks<us>(pushes & safe) & nonPawnEnemies;
    TRACE_ADD(pushThreat, pushThreats.popcount());
}

template<Color us>
void evalKingPawnFile(uint32_t file, Bitboard ourPawns, Bitboard theirPawns, uint32_t theirKing, Trace& trace)
{
    uint32_t kingFile = fileOf(theirKing);
    {
        Bitboard filePawns = ourPawns & Bitboard::fileBB(file);
        // 4 = e file
        int idx = (kingFile == file) ? 1 : (kingFile >= 4) == (kingFile < file) ? 0 : 2;

        int rankDist = filePawns.any() ?
            std::abs(rankOf(us == Color::WHITE ? filePawns.msb() : filePawns.lsb()) - rankOf(theirKing)) :
            7;
        TRACE_INC(pawnStorm[idx][rankDist]);
    }
    {
        Bitboard filePawns = theirPawns & Bitboard::fileBB(file);
        // 4 = e file
        int idx = (kingFile == file) ? 1 : (kingFile >= 4) == (kingFile < file) ? 0 : 2;
        int rankDist = filePawns.any() ?
            std::abs(rankOf(us == Color::WHITE ? filePawns.msb() : filePawns.lsb()) - rankOf(theirKing)) :
            7;
        TRACE_INC(pawnShield[idx][rankDist]);
    }
}

template<Color us>
void evaluateKings(const Board& board, const EvalData& evalData, Trace& trace)
{
    constexpr Color them = ~us;
    Bitboard ourPawns = board.pieces(us, PieceType::PAWN);
    Bitboard theirPawns = board.pieces(them, PieceType::PAWN);

    uint32_t theirKing = board.pieces(them, PieceType::KING).lsb();

    for (uint32_t file = 0; file < 8; file++)
        evalKingPawnFile<us>(file, ourPawns, theirPawns, theirKing, trace);

    Bitboard rookCheckSquares = attacks::rookAttacks(theirKing, board.allPieces());
    Bitboard bishopCheckSquares = attacks::rookAttacks(theirKing, board.allPieces());

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
    Bitboard whitePawns = board.pieces(Color::WHITE, PieceType::PAWN);
    Bitboard blackPawns = board.pieces(Color::BLACK, PieceType::PAWN);
    Bitboard whitePawnAttacks = attacks::pawnAttacks<Color::WHITE>(whitePawns);
    Bitboard blackPawnAttacks = attacks::pawnAttacks<Color::BLACK>(blackPawns);
    uint32_t whiteKing = board.pieces(Color::WHITE, PieceType::KING).lsb();
    uint32_t blackKing = board.pieces(Color::BLACK, PieceType::KING).lsb();

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
            Bitboard pieces = board.pieces(c, pt);
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

    addCoefficientArray(trace.threatByPawn);
    addCoefficientArray2D(trace.threatByKnight);
    addCoefficientArray2D(trace.threatByBishop);
    addCoefficientArray2D(trace.threatByRook);
    addCoefficientArray2D(trace.threatByQueen);
    addCoefficientArray(trace.threatByKing);
    addCoefficient(trace.pushThreat);

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
    addCoefficientArray(trace.bishopPawns);
    addCoefficient(trace.bishopPair);
    addCoefficientArray(trace.openRook);

    addCoefficient(trace.tempo);
    return {pos, m_Coefficients.size()};
}

using InitialParam = std::array<int, 2>;

#define S(mg, eg) {mg, eg}

constexpr InitialParam MATERIAL[6] = {S(  62,   87), S( 294,  362), S( 291,  378), S( 412,  671), S( 860, 1227), S(0, 0)};

constexpr InitialParam PSQT[6][64] = {
	{
		S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
		S(  36,   77), S(  43,   79), S(  28,   86), S(  63,   45), S(  48,   51), S(  37,   59), S( -28,   98), S( -31,   91),
		S(  14,   40), S(   9,   56), S(  36,   14), S(  45,  -15), S(  51,  -15), S(  84,   -6), S(  44,   38), S(  29,   28),
		S(  -7,   23), S(  -3,   27), S(   5,    3), S(   5,   -9), S(  24,   -9), S(  26,  -10), S(  10,   17), S(  11,    2),
		S( -15,    6), S( -12,   21), S(  -1,   -4), S(   7,   -8), S(   9,   -7), S(  13,  -11), S(   0,    9), S(  -1,  -11),
		S( -25,   -1), S( -20,   10), S( -11,   -6), S(  -9,   -3), S(   1,   -3), S(  -5,   -8), S(   4,   -2), S(  -7,  -17),
		S( -18,    4), S( -11,   15), S(  -4,    0), S(  -5,   -2), S(   3,    8), S(  27,   -9), S(  18,    1), S(  -4,  -15),
		S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
	},
	{
		S( -94,  -34), S( -83,   -7), S( -61,    8), S( -14,   -8), S(   7,   -4), S(  -5,  -27), S( -69,   -4), S( -70,  -50),
		S( -13,    6), S(  -2,   10), S(   1,    2), S(  10,    1), S(   7,   -6), S(  29,  -13), S(  15,    2), S(   6,   -9),
		S(   1,    0), S(   4,    2), S(  10,    3), S(  16,    3), S(  28,   -3), S(  57,  -22), S(  13,   -7), S(  22,  -11),
		S(   8,   11), S(  16,    3), S(  28,    4), S(  35,    9), S(  29,   13), S(  44,    3), S(  31,    5), S(  38,    1),
		S(   2,   12), S(  18,    3), S(  18,   13), S(  27,   11), S(  21,   23), S(  25,    7), S(  24,    8), S(   8,   15),
		S( -15,    0), S(  -4,    0), S(  -2,   -1), S(   3,   13), S(  16,   11), S(   1,   -6), S(  15,   -4), S(   0,    4),
		S( -19,    6), S( -16,   10), S( -10,    0), S(   2,    0), S(   1,   -1), S(   5,   -1), S(   6,   -2), S(   1,   17),
		S( -49,   22), S( -12,   -4), S( -20,    0), S( -11,    3), S(  -4,    4), S(   0,   -7), S( -11,    5), S( -16,   17),
	},
	{
		S(  -9,    2), S( -37,    8), S( -42,    2), S( -67,    8), S( -68,    7), S( -24,   -5), S( -26,   -1), S( -49,   -1),
		S(  -8,   -6), S( -13,   -2), S(  -6,   -4), S( -21,    0), S(  -9,   -7), S( -19,   -4), S( -52,   10), S( -39,    1),
		S(   2,    5), S(  12,    1), S(   1,    2), S(  16,   -5), S(   2,    0), S(  34,    6), S(  15,    0), S(  21,    6),
		S(  -6,    5), S(   6,    6), S(  13,    4), S(  20,   15), S(  14,   11), S(  11,    9), S(   7,    9), S(   2,    3),
		S(   2,    1), S(   0,    8), S(   4,   10), S(  16,   12), S(  15,    9), S(   8,    6), S(   5,    5), S(  18,   -7),
		S(   1,    1), S(  14,    2), S(   7,    4), S(   0,    6), S(   7,   10), S(   8,    0), S(  18,   -7), S(  16,  -10),
		S(  19,    4), S(   8,  -13), S(  12,  -13), S(  -3,    1), S(   4,   -2), S(  18,  -10), S(  25,  -15), S(  23,  -13),
		S(  12,   -4), S(  15,    8), S(   5,    3), S(  -1,   -3), S(  13,   -6), S(  -2,    7), S(  23,  -11), S(  33,  -23),
	},
	{
		S( -12,   16), S( -16,   20), S( -24,   29), S( -29,   26), S( -16,   19), S(   5,   16), S(  -1,   21), S(  20,   11),
		S(  -3,   12), S(   1,   17), S(   9,   19), S(  24,    7), S(  10,    9), S(  15,    8), S(  25,    3), S(  29,    1),
		S( -10,   11), S(  16,    6), S(   7,    9), S(   7,    4), S(  26,   -2), S(  34,   -9), S(  49,   -7), S(  19,   -6),
		S(  -9,   13), S(   9,    6), S(   8,   10), S(   4,    6), S(  15,   -4), S(  19,   -7), S(  15,    0), S(   8,   -3),
		S( -16,    7), S(  -8,    7), S(  -5,    5), S(   0,    3), S(   6,    1), S(  -5,    5), S(  12,   -1), S(  -8,   -2),
		S( -23,    1), S( -18,   -3), S( -14,   -5), S( -11,   -4), S(  -2,   -8), S(  -6,  -10), S(  16,  -22), S(  -1,  -21),
		S( -25,   -5), S( -19,   -5), S( -10,   -7), S(  -8,   -9), S(  -4,  -15), S(   0,  -16), S(   6,  -20), S( -20,  -16),
		S( -17,   -5), S( -16,   -9), S( -15,   -4), S(  -7,  -12), S(  -1,  -17), S(  -6,  -11), S(  -7,  -13), S( -20,  -12),
	},
	{
		S( -20,   -9), S( -32,    2), S( -25,   23), S(   2,   10), S(  -4,   13), S(   7,    7), S(  49,  -39), S(  10,   -9),
		S(   5,  -15), S( -15,   -2), S( -12,   25), S( -25,   43), S( -27,   57), S(   1,   23), S(  -3,   12), S(  40,    8),
		S(   8,   -5), S(   1,   -2), S(  -5,   22), S(  -4,   27), S( -15,   40), S(   6,   11), S(  23,   -4), S(  27,   -2),
		S(   1,    7), S(   2,   15), S(   5,   12), S( -10,   27), S( -13,   30), S(  10,   15), S(   9,   30), S(  17,   12),
		S(   0,    4), S(   3,   16), S(  -1,   13), S(  -5,   21), S(   6,   22), S(   3,   22), S(  17,    6), S(  16,    9),
		S(   2,  -16), S(   1,   -4), S(  -8,    7), S(  -8,   14), S(  -3,   19), S(   1,    5), S(  14,  -13), S(  14,  -17),
		S(   3,  -30), S(  -2,  -31), S(   2,  -22), S(   2,   -7), S(   1,   -6), S(   8,  -32), S(  17,  -63), S(  29,  -76),
		S( -14,  -28), S( -15,  -24), S( -13,  -13), S( -10,  -13), S(  -9,  -14), S( -17,  -24), S(  -6,  -39), S(  10,  -48),
	},
	{
		S(  40,  -51), S(  80,  -30), S( 118,  -33), S(  52,  -10), S(  59,  -33), S( -26,    3), S(  17,    6), S(  86,  -66),
		S( -97,   25), S(  15,   22), S(  29,   11), S( 147,  -18), S(  88,  -10), S(  48,   23), S(  21,   38), S( -86,   44),
		S(-110,   24), S(  53,   12), S(  38,    7), S(  46,    2), S(  84,    0), S(  99,   10), S(   7,   34), S( -62,   29),
		S( -64,    1), S( -19,    5), S( -15,    6), S( -16,    1), S( -20,   -3), S( -30,   15), S( -56,   22), S(-155,   30),
		S( -72,   -9), S( -27,   -1), S( -34,    4), S( -33,    2), S( -35,    1), S( -37,    5), S( -69,   14), S(-151,   20),
		S( -52,   -4), S(   3,   -2), S( -22,    3), S(  -6,   -1), S(  -9,   -2), S( -25,    6), S( -18,    4), S( -75,   11),
		S(  31,  -14), S(  22,    0), S(  18,   -1), S(   0,   -3), S(  -4,    0), S(   6,    2), S(  22,   -1), S(  16,   -9),
		S(  21,  -33), S(  54,  -24), S(  26,   -8), S( -28,   -1), S(   8,  -17), S( -21,    4), S(  33,  -15), S(  30,  -38),
	},
};

constexpr InitialParam MOBILITY[4][28] = {
	{S( -10,  -13), S( -34,  -60), S( -13,  -29), S(  -3,   -5), S(   7,    6), S(  11,   17), S(  20,   23), S(  29,   28), S(  38,   26), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)},
	{S(  -4,  -32), S( -40,  -92), S( -18,  -44), S( -11,  -21), S(   1,   -9), S(   8,    1), S(  13,   11), S(  18,   16), S(  21,   21), S(  25,   23), S(  24,   26), S(  34,   19), S(  27,   25), S(  42,   13), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)},
	{S( -45,  -25), S(   1,  -54), S( -27,  -47), S( -17,  -32), S( -11,  -26), S(  -5,  -11), S(  -4,   -4), S(  -7,    2), S(  -3,    4), S(   1,    8), S(   6,   12), S(   8,   17), S(  13,   21), S(  20,   22), S(  24,   18), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)},
	{S( -48,    5), S( -64,  -73), S(-110,  -62), S( -77, -267), S( -82, -110), S( -37,  -55), S( -26,  -31), S( -18,  -26), S( -11,  -16), S(  -9,    6), S(  -6,   14), S(  -2,   23), S(   1,   29), S(   5,   31), S(   8,   34), S(  11,   38), S(  12,   40), S(  12,   45), S(  13,   49), S(  13,   52), S(  20,   48), S(  27,   36), S(  30,   38), S(  56,   10), S(  59,   13), S( 103,  -24), S(  99,  -24), S(  56,  -24)}
};

constexpr InitialParam THREAT_BY_PAWN[6] = {S(   7,  -16), S(  64,   27), S(  64,   56), S(  89,    8), S(  75,  -21), S(   0,    0)};
constexpr InitialParam THREAT_BY_KNIGHT[2][6] = {
	{S(   8,   27), S(  30,   21), S(  36,   37), S(  76,    2), S(  43,  -28), S(   0,    0)},
	{S(  -3,   11), S(  13,   41), S(  29,   31), S(  62,   26), S(  52,   -2), S(   0,    0)}
};
constexpr InitialParam THREAT_BY_BISHOP[2][6] = {
	{S(   5,   32), S(  50,   22), S(   2,   33), S(  71,    7), S(  71,   32), S(   0,    0)},
	{S(   1,    9), S(  23,   23), S( -13,   12), S(  44,   40), S(  51,  121), S(   0,    0)}
};
constexpr InitialParam THREAT_BY_ROOK[2][6] = {
	{S(   1,   41), S(  30,   47), S(  29,   46), S(  11,  -26), S(  75,   -7), S(   0,    0)},
	{S(  -6,   14), S(   5,   17), S(  16,    7), S(   6,  -51), S(  57,   53), S(   0,    0)}
};
constexpr InitialParam THREAT_BY_QUEEN[2][6] = {
	{S(  12,    4), S(  37,    5), S(  19,   42), S(  25,  -11), S(  13,  -69), S( 155,  -59)},
	{S(  -1,   15), S(   2,    7), S(  -4,   23), S(  -3,    3), S( -19,  -55), S( 116,   52)}
};
constexpr InitialParam THREAT_BY_KING[6] = {S( -16,   39), S(  29,   40), S(  30,   45), S( 100,    7), S(   0,    0), S(   0,    0)};
constexpr InitialParam PUSH_THREAT = S(  19,   16);

constexpr InitialParam PASSED_PAWN[8] = {S(   0,    0), S( -17,  -69), S( -16,  -54), S( -14,  -20), S(  10,   16), S(  -1,   85), S(  48,   95), S(   0,    0)};
constexpr InitialParam ISOLATED_PAWN[8] = {S(  -4,    7), S(  -2,  -11), S( -11,   -5), S(  -6,  -13), S( -11,  -13), S(  -7,   -3), S(  -1,  -11), S( -10,    7)};
constexpr InitialParam PAWN_PHALANX[8] = {S(   0,    0), S(   5,   -2), S(  12,    6), S(  18,   15), S(  45,   54), S( 118,  178), S(-147,  433), S(   0,    0)};
constexpr InitialParam DEFENDED_PAWN[8] = {S(   0,    0), S(   0,    0), S(  18,   10), S(  12,    7), S(  14,   15), S(  30,   36), S( 174,   22), S(   0,    0)};
constexpr InitialParam OUR_PASSER_PROXIMITY[8] = {S(   0,    0), S(   6,   52), S(  -6,   40), S(  -5,   24), S(  -2,   14), S(   2,   11), S(  17,    8), S(   6,    7)};
constexpr InitialParam THEIR_PASSER_PROXIMITY[8] = {S(   0,    0), S( -79,    0), S(   7,   -0), S(  -1,   27), S(   5,   37), S(   5,   45), S(   8,   50), S( -10,   49)};

constexpr InitialParam PAWN_STORM[3][8] = {
	{S(  42,  -31), S(  22,  -16), S(  17,   -5), S(   8,   -1), S(   1,    2), S(  -3,    6), S(  -1,    5), S(   6,  -11)},
	{S(   0,    0), S(  16,  -26), S(  27,   -2), S(   4,    1), S(  -5,    7), S(  -6,   13), S(  -7,   14), S(   7,   -3)},
	{S(   0,   -2), S(  -8,    6), S(   3,    8), S(   2,    9), S(   4,    8), S(   4,   10), S(   4,   10), S(  -9,   -4)}
};
constexpr InitialParam PAWN_SHIELD[3][8] = {
	{S(   2,  -13), S(  -8,   -7), S(  -4,  -11), S(   1,  -11), S(  14,  -14), S(  17,  -25), S(  27,  -29), S(   7,    0)},
	{S(   0,    0), S( -15,    3), S(  -8,   -4), S(   9,   -4), S(  20,  -10), S(  34,  -29), S(  53,  -41), S(  16,    2)},
	{S(  -3,   -8), S(  -2,   -5), S(   0,   -5), S(   1,   -0), S(   4,    1), S(   5,    2), S(  22,    6), S(  -9,    7)}
};
constexpr InitialParam SAFE_KNIGHT_CHECK = S(  79,   -4);
constexpr InitialParam SAFE_BISHOP_CHECK = S(  16,   -7);
constexpr InitialParam SAFE_ROOK_CHECK = S(  58,   -4);
constexpr InitialParam SAFE_QUEEN_CHECK = S(  31,   10);
constexpr InitialParam KING_ATTACKER_WEIGHT[4] = {S(  15,   -4), S(   8,   -1), S(   7,  -18), S(  -2,   11)};
constexpr InitialParam KING_ATTACKS[14] = {S( -26,    6), S( -29,    4), S( -30,    2), S( -28,    7), S( -19,    5), S(  -4,    2), S(  20,   -7), S(  49,  -18), S(  94,  -38), S( 124,  -44), S( 161,  -60), S( 185,  -47), S( 220, -128), S( 184,    9)};

constexpr InitialParam KNIGHT_OUTPOST = S(  24,   17);
constexpr InitialParam BISHOP_PAWNS[7] = {S(  22,   19), S(  24,   17), S(  21,   10), S(  17,    2), S(  12,   -7), S(  10,  -19), S(   4,  -31)};
constexpr InitialParam BISHOP_PAIR = S(  20,   60);
constexpr InitialParam ROOK_OPEN[2] = {S(  21,    9), S(  11,   11)};

constexpr InitialParam TEMPO = S(  29,   30);

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

    addEvalParamArray(params, THREAT_BY_PAWN);
    addEvalParamArray2D(params, THREAT_BY_KNIGHT);
    addEvalParamArray2D(params, THREAT_BY_BISHOP);
    addEvalParamArray2D(params, THREAT_BY_ROOK);
    addEvalParamArray2D(params, THREAT_BY_QUEEN);
    addEvalParamArray(params, THREAT_BY_KING);
    addEvalParam(params, PUSH_THREAT);

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
    addEvalParamArray(params, BISHOP_PAWNS);
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

    state.ss << '\n';

    state.ss << "constexpr InitialParam THREAT_BY_PAWN[6] = ";
    printArray<ALIGN_SIZE>(state, 6);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam THREAT_BY_KNIGHT[2][6] = ";
    printArray2D<ALIGN_SIZE>(state, 2, 6);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam THREAT_BY_BISHOP[2][6] = ";
    printArray2D<ALIGN_SIZE>(state, 2, 6);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam THREAT_BY_ROOK[2][6] = ";
    printArray2D<ALIGN_SIZE>(state, 2, 6);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam THREAT_BY_QUEEN[2][6] = ";
    printArray2D<ALIGN_SIZE>(state, 2, 6);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam THREAT_BY_KING[6] = ";
    printArray<ALIGN_SIZE>(state, 6);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam PUSH_THREAT = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << '\n';

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

    state.ss << '\n';

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

    state.ss << '\n';

    state.ss << "constexpr InitialParam KNIGHT_OUTPOST = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam BISHOP_PAWNS[7] = ";
    printArray<ALIGN_SIZE>(state, 7);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam BISHOP_PAIR = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam ROOK_OPEN[2] = ";
    printArray<ALIGN_SIZE>(state, 2);
    state.ss << ";\n";

    state.ss << '\n';

    state.ss << "constexpr InitialParam TEMPO = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";
}

void EvalFn::printEvalParams(const EvalParams& params, std::ostream& os)
{
    PrintState state{params, 0};
    printPSQTs<0>(state);
    state.ss << '\n';
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
    state.ss << '\n';
    printPSQTs<4>(state);
    state.ss << '\n';
    printRestParams<4>(state);
    os << state.ss.str() << std::endl;
}
