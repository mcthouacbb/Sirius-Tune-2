#include "eval_fn.h"
#include "sirius/attacks.h"
#include "eval_constants.h"

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

#define TRACE_OFFSET(elem) (offsetof(Trace, elem) / sizeof(TraceElem))
#define TRACE_SIZE(elem) (sizeof(Trace::elem) / sizeof(TraceElem))

#define TRACE_INC(traceElem) trace.traceElem[us]++
#define TRACE_ADD(traceElem, amount) trace.traceElem[us] += amount

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

    TraceElem isolatedPawn[8];
    TraceElem doubledPawn[8];
    TraceElem backwardsPawn[8];
    TraceElem pawnPhalanx[8];
    TraceElem defendedPawn[8];

    TraceElem passedPawn[2][2][8];
    TraceElem ourPasserProximity[8];
    TraceElem theirPasserProximity[8];

    TraceElem pawnStorm[2][4][8];
    TraceElem pawnShield[4][8];
    TraceElem safeKnightCheck;
    TraceElem safeBishopCheck;
    TraceElem safeRookCheck;
    TraceElem safeQueenCheck;
    TraceElem unsafeKnightCheck;
    TraceElem unsafeBishopCheck;
    TraceElem unsafeRookCheck;
    TraceElem unsafeQueenCheck;
    TraceElem kingAttackerWeight[4];
    TraceElem kingAttacks[14];
    TraceElem weakKingRing[9];

    TraceElem minorBehindPawn;
    TraceElem knightOutpost;
    TraceElem bishopPawns[7];
    TraceElem bishopPair;
    TraceElem openRook[2];

    TraceElem tempo;

    TraceElem complexityPawns;
    TraceElem complexityPassers;
    TraceElem complexityPawnsBothSides;
    TraceElem complexityPawnEndgame;
    TraceElem complexityOffset;

    double egScale;
};

struct EvalData
{
    ColorArray<Bitboard> mobilityArea;
    ColorArray<Bitboard> attacked;
    ColorArray<Bitboard> attackedBy2;
    ColorArray<PieceTypeArray<Bitboard>> attackedBy;
    ColorArray<Bitboard> kingRing;
    ColorArray<PackedScore> attackWeight;
    ColorArray<int> attackCount;
};

struct PawnStructure
{
    PawnStructure() = default;
    PawnStructure(const Board& board)
    {
        Bitboard wpawns = board.pieces(Color::WHITE, PieceType::PAWN);
        Bitboard bpawns = board.pieces(Color::BLACK, PieceType::PAWN);
        pawnAttacks[Color::WHITE] = attacks::pawnAttacks<Color::WHITE>(wpawns);
        pawnAttackSpans[Color::WHITE] = attacks::fillUp<Color::WHITE>(pawnAttacks[Color::WHITE]);
        passedPawns = Bitboard(0);

        pawnAttacks[Color::BLACK] = attacks::pawnAttacks<Color::BLACK>(bpawns);
        pawnAttackSpans[Color::BLACK] = attacks::fillUp<Color::BLACK>(pawnAttacks[Color::BLACK]);
        passedPawns = Bitboard(0);
    }

    ColorArray<Bitboard> pawnAttacks;
    ColorArray<Bitboard> pawnAttackSpans;
    Bitboard passedPawns;
};

template<Color us>
PackedScore evaluateKnightOutposts(const Board& board, const PawnStructure& pawnStructure, Trace& trace)
{
    constexpr Color them = ~us;
    Bitboard outpostRanks = RANK_4_BB | RANK_5_BB | (us == Color::WHITE ? RANK_6_BB : RANK_3_BB);
    Bitboard outposts = outpostRanks & ~pawnStructure.pawnAttackSpans[them] & pawnStructure.pawnAttacks[us];
    TRACE_ADD(knightOutpost, (board.pieces(us, PieceType::KNIGHT) & outposts).popcount());
    return KNIGHT_OUTPOST * (board.pieces(us, PieceType::KNIGHT) & outposts).popcount();
}

template<Color us>
PackedScore evaluateBishopPawns(const Board& board, Trace& trace)
{
    Bitboard bishops = board.pieces(us, PieceType::BISHOP);

    PackedScore eval{0, 0};
    while (bishops.any())
    {
        Square sq = bishops.poplsb();
        bool lightSquare = (Bitboard::fromSquare(sq) & LIGHT_SQUARES_BB).any();
        Bitboard sameColorPawns = board.pieces(us, PieceType::PAWN) & (lightSquare ? LIGHT_SQUARES_BB : DARK_SQUARES_BB);
        TRACE_INC(bishopPawns[std::min(sameColorPawns.popcount(), 6u)]);
        eval += BISHOP_PAWNS[std::min(sameColorPawns.popcount(), 6u)];
    }
    return eval;
}

template<Color us>
PackedScore evaluateRookOpen(const Board& board, Trace& trace)
{
    constexpr Color them = ~us;
    Bitboard ourPawns = board.pieces(us, PieceType::PAWN);
    Bitboard theirPawns = board.pieces(them, PieceType::PAWN);
    Bitboard rooks = board.pieces(us, PieceType::ROOK);

    PackedScore eval{0, 0};
    while (rooks.any())
    {
        Bitboard fileBB = Bitboard::fileBB(rooks.poplsb().file());
        if ((ourPawns & fileBB).empty())
            eval += (theirPawns & fileBB).any() ? ROOK_OPEN[1] : ROOK_OPEN[0];

        if ((ourPawns & fileBB).empty())
        {
            if ((theirPawns & fileBB).any())
                TRACE_INC(openRook[1]);
            else
                TRACE_INC(openRook[0]);
        }
    }
    return eval;
}

template<Color us>
PackedScore evaluateMinorBehindPawn(const Board& board, Trace& trace)
{
    Bitboard pawns = board.pieces(PieceType::PAWN);
    Bitboard minors = board.pieces(us, PieceType::KNIGHT) | board.pieces(us, PieceType::BISHOP);

    Bitboard shielded = minors & (us == Color::WHITE ? pawns.south() : pawns.north());
    TRACE_ADD(minorBehindPawn, shielded.popcount());
    return MINOR_BEHIND_PAWN * shielded.popcount();
}

template<Color us, PieceType piece>
PackedScore evaluatePieces(const Board& board, EvalData& evalData, Trace& trace)
{
    constexpr Color them = ~us;
    Bitboard ourPawns = board.pieces(us, PieceType::PAWN);
    Bitboard theirPawns = board.pieces(them, PieceType::PAWN);

    PackedScore eval{0, 0};
    Bitboard pieces = board.pieces(us, piece);
    if constexpr (piece == PieceType::BISHOP)
        if (pieces.multiple())
        {
            eval += BISHOP_PAIR;
            TRACE_INC(bishopPair);
        }

    Bitboard occupancy = board.allPieces();
    if constexpr (piece == PieceType::BISHOP)
        occupancy ^= board.pieces(us, PieceType::BISHOP) | board.pieces(us, PieceType::QUEEN);
    else if constexpr (piece == PieceType::ROOK)
        occupancy ^= board.pieces(us, PieceType::ROOK) | board.pieces(us, PieceType::QUEEN);
    else if constexpr (piece == PieceType::QUEEN)
        occupancy ^= board.pieces(us, PieceType::BISHOP) | board.pieces(us, PieceType::ROOK);

    Bitboard outpostSquares = RANK_4_BB | RANK_5_BB | (us == Color::WHITE ? RANK_6_BB : RANK_3_BB);

    while (pieces.any())
    {
        Square sq = pieces.poplsb();
        Bitboard attacks = attacks::pieceAttacks<piece>(sq, occupancy);
        if ((board.checkBlockers(us) & Bitboard::fromSquare(sq)).any())
            attacks &= attacks::inBetweenSquares(sq, board.pieces(us, PieceType::KING).lsb());

        evalData.attackedBy[us][piece] |= attacks;
        evalData.attackedBy2[us] |= evalData.attacked[us] & attacks;
        evalData.attacked[us] |= attacks;

        eval += MOBILITY[static_cast<int>(piece) - static_cast<int>(PieceType::KNIGHT)][(attacks & evalData.mobilityArea[us]).popcount()];
        TRACE_INC(mobility[static_cast<int>(piece) - static_cast<int>(PieceType::KNIGHT)][(attacks & evalData.mobilityArea[us]).popcount()]);

        if (Bitboard kingRingAtks = evalData.kingRing[them] & attacks; kingRingAtks.any())
        {
            evalData.attackWeight[us] += KING_ATTACKER_WEIGHT[static_cast<int>(piece) - static_cast<int>(PieceType::KNIGHT)];
            TRACE_INC(kingAttackerWeight[static_cast<int>(piece) - static_cast<int>(PieceType::KNIGHT)]);
            evalData.attackCount[us] += kingRingAtks.popcount();
        }
    }

    return eval;
}



template<Color us>
PackedScore evaluatePawns(const Board& board, PawnStructure& pawnStructure, Trace& trace)
{
    constexpr Color them = ~us;
    Bitboard ourPawns = board.pieces(us, PieceType::PAWN);
    Bitboard theirPawns = board.pieces(them, PieceType::PAWN);

    PackedScore eval{0, 0};

    Bitboard pawns = ourPawns;
    while (pawns.any())
    {
        Square sq = pawns.poplsb();
        Square push = sq + attacks::pawnPushOffset<us>();
        Bitboard attacks = attacks::pawnAttacks(us, sq);
        Bitboard threats = attacks & theirPawns;
        Bitboard pushThreats = attacks::pawnPushes<us>(attacks) & theirPawns;
        Bitboard support = attacks::passedPawnMask(them, push) & attacks::isolatedPawnMask(sq) & ourPawns;

        bool blocked = theirPawns.has(push);
        bool doubled = ourPawns.has(push);
        bool backwards = (blocked || pushThreats.any()) && support.empty();

        if (board.isPassedPawn(sq))
            pawnStructure.passedPawns |= Bitboard::fromSquare(sq);

        if (doubled && threats.empty())
        {
            eval += DOUBLED_PAWN[sq.file()];
            TRACE_INC(doubledPawn[sq.file()]);
        }

        if (threats.empty() && board.isIsolatedPawn(sq))
        {
            eval += ISOLATED_PAWN[sq.file()];
            TRACE_INC(isolatedPawn[sq.file()]);
        }
        else if (backwards)
        {
            eval += BACKWARDS_PAWN[sq.relativeRank<us>()];
            TRACE_INC(backwardsPawn[sq.relativeRank<us>()]);
        }
    }

    Bitboard phalanx = ourPawns & ourPawns.west();
    while (phalanx.any())
    {
        Square sq = phalanx.poplsb();
        eval += PAWN_PHALANX[sq.relativeRank<us>()];
        TRACE_INC(pawnPhalanx[sq.relativeRank<us>()]);
    }

    Bitboard defended = ourPawns & attacks::pawnAttacks<us>(ourPawns);
    while (defended.any())
    {
        Square sq = defended.poplsb();
        eval += DEFENDED_PAWN[sq.relativeRank<us>()];
        TRACE_INC(defendedPawn[sq.relativeRank<us>()]);
    }

    return eval;
}

template<Color us>
PackedScore evaluatePassedPawns(const Board & board, const PawnStructure& pawnStructure, const EvalData& evalData, Trace& trace)
{
    constexpr Color them = ~us;
    Square ourKing = board.kingSq(us);
    Square theirKing = board.kingSq(them);

    Bitboard passers = pawnStructure.passedPawns & board.pieces(us);

    PackedScore eval{0, 0};

    while (passers.any())
    {
        Square passer = passers.poplsb();
        int rank = passer.relativeRank<us>();
        if (rank >= RANK_4)
        {
            Square pushSq = passer + attacks::pawnPushOffset<us>();

            bool blocked = board.pieceAt(pushSq) != Piece::NONE;
            bool controlled = (evalData.attacked[them] & Bitboard::fromSquare(pushSq)).any();
            eval += PASSED_PAWN[blocked][controlled][rank];
            TRACE_INC(passedPawn[blocked][controlled][rank]);

            eval += OUR_PASSER_PROXIMITY[Square::chebyshev(ourKing, passer)];
            eval += THEIR_PASSER_PROXIMITY[Square::chebyshev(theirKing, passer)];
            TRACE_INC(ourPasserProximity[Square::chebyshev(ourKing, passer)]);
            TRACE_INC(theirPasserProximity[Square::chebyshev(theirKing, passer)]);
        }
    }

    return eval;
}

PackedScore evaluatePawns(const Board& board, PawnStructure& pawnStructure, Trace& trace)
{
    return evaluatePawns<Color::WHITE>(board, pawnStructure, trace) - evaluatePawns<Color::BLACK>(board, pawnStructure, trace);
}

template<Color us>
PackedScore evaluateThreats(const Board& board, const EvalData& evalData, Trace& trace)
{
    constexpr Color them = ~us;

    PackedScore eval{0, 0};

    Bitboard defendedBB =
        evalData.attackedBy2[them] |
        evalData.attackedBy[them][PieceType::PAWN] |
        (evalData.attacked[them] & ~evalData.attackedBy2[us]);

    Bitboard pawnThreats = evalData.attackedBy[us][PieceType::PAWN] & board.pieces(them);
    while (pawnThreats.any())
    {
        PieceType threatened = getPieceType(board.pieceAt(pawnThreats.poplsb()));
        eval += THREAT_BY_PAWN[static_cast<int>(threatened)];
        TRACE_INC(threatByPawn[static_cast<int>(threatened)]);
    }

    Bitboard knightThreats = evalData.attackedBy[us][PieceType::KNIGHT] & board.pieces(them);
    while (knightThreats.any())
    {
        Square threat = knightThreats.poplsb();
        PieceType threatened = getPieceType(board.pieceAt(threat));
        bool defended = (defendedBB & Bitboard::fromSquare(threat)).any();
        eval += THREAT_BY_KNIGHT[defended][static_cast<int>(threatened)];
        TRACE_INC(threatByKnight[defended][static_cast<int>(threatened)]);
    }

    Bitboard bishopThreats = evalData.attackedBy[us][PieceType::BISHOP] & board.pieces(them);
    while (bishopThreats.any())
    {
        Square threat = bishopThreats.poplsb();
        PieceType threatened = getPieceType(board.pieceAt(threat));
        bool defended = (defendedBB & Bitboard::fromSquare(threat)).any();
        eval += THREAT_BY_BISHOP[defended][static_cast<int>(threatened)];
        TRACE_INC(threatByBishop[defended][static_cast<int>(threatened)]);
    }

    Bitboard rookThreats = evalData.attackedBy[us][PieceType::ROOK] & board.pieces(them);
    while (rookThreats.any())
    {
        Square threat = rookThreats.poplsb();
        PieceType threatened = getPieceType(board.pieceAt(threat));
        bool defended = (defendedBB & Bitboard::fromSquare(threat)).any();
        eval += THREAT_BY_ROOK[defended][static_cast<int>(threatened)];
        TRACE_INC(threatByRook[defended][static_cast<int>(threatened)]);
    }

    Bitboard queenThreats = evalData.attackedBy[us][PieceType::QUEEN] & board.pieces(them);
    while (queenThreats.any())
    {
        Square threat = queenThreats.poplsb();
        PieceType threatened = getPieceType(board.pieceAt(threat));
        bool defended = (defendedBB & Bitboard::fromSquare(threat)).any();
        eval += THREAT_BY_QUEEN[defended][static_cast<int>(threatened)];
        TRACE_INC(threatByQueen[defended][static_cast<int>(threatened)]);
    }

    Bitboard kingThreats = evalData.attackedBy[us][PieceType::KING] & board.pieces(them) & ~defendedBB;
    while (kingThreats.any())
    {
        PieceType threatened = getPieceType(board.pieceAt(kingThreats.poplsb()));
        eval += THREAT_BY_KING[static_cast<int>(threatened)];
        TRACE_INC(threatByKing[static_cast<int>(threatened)]);
    }


    Bitboard nonPawnEnemies = board.pieces(them) & ~board.pieces(PieceType::PAWN);

    Bitboard safe = ~defendedBB | (evalData.attacked[us] & ~evalData.attackedBy[them][PieceType::PAWN] & ~evalData.attackedBy2[them]);
    Bitboard pushes = attacks::pawnPushes<us>(board.pieces(us, PieceType::PAWN)) & ~board.allPieces();
    pushes |= attacks::pawnPushes<us>(pushes & Bitboard::nthRank<us, RANK_3>()) & ~board.allPieces();

    Bitboard pushThreats = attacks::pawnAttacks<us>(pushes & safe) & nonPawnEnemies;
    eval += PUSH_THREAT * pushThreats.popcount();
    TRACE_ADD(pushThreat, pushThreats.popcount());

    return eval;
}

template<Color us>
PackedScore evalKingPawnFile(uint32_t file, Bitboard ourPawns, Bitboard theirPawns, Trace& trace)
{
    constexpr Color them = ~us;

    PackedScore eval{0, 0};
    int edgeDist = std::min(file, 7 - file);
    {
        Bitboard filePawns = ourPawns & Bitboard::fileBB(file);
        int rank = filePawns.any() ?
            (us == Color::WHITE ? filePawns.msb() : filePawns.lsb()).relativeRank<them>() :
            0;
        bool blocked = (theirPawns & Bitboard::fromSquare(Square(rank + attacks::pawnPushOffset<us>(), file))).any();
        eval += PAWN_STORM[blocked][edgeDist][rank];
        TRACE_INC(pawnStorm[blocked][edgeDist][rank]);
    }
    {
        Bitboard filePawns = theirPawns & Bitboard::fileBB(file);
        int rank = filePawns.any() ?
            (us == Color::WHITE ? filePawns.msb() : filePawns.lsb()).relativeRank<them>() :
            0;
        eval += PAWN_SHIELD[edgeDist][rank];
        TRACE_INC(pawnShield[edgeDist][rank]);
    }

    return eval;
}

template<Color us>
PackedScore evaluateKings(const Board& board, const EvalData& evalData, Trace& trace)
{
    constexpr Color them = ~us;
    Bitboard ourPawns = board.pieces(us, PieceType::PAWN);
    Bitboard theirPawns = board.pieces(them, PieceType::PAWN);

    Square theirKing = board.kingSq(them);

    PackedScore eval{0, 0};

    uint32_t leftFile = std::clamp(theirKing.file() - 1, FILE_A, FILE_F);
    uint32_t rightFile = std::clamp(theirKing.file() + 1, FILE_C, FILE_H);
    for (uint32_t file = leftFile; file <= rightFile; file++)
        eval += evalKingPawnFile<us>(file, ourPawns, theirPawns, trace);

    Bitboard rookCheckSquares = attacks::rookAttacks(theirKing, board.allPieces());
    Bitboard bishopCheckSquares = attacks::bishopAttacks(theirKing, board.allPieces());

    Bitboard knightChecks = evalData.attackedBy[us][PieceType::KNIGHT] & attacks::knightAttacks(theirKing);
    Bitboard bishopChecks = evalData.attackedBy[us][PieceType::BISHOP] & bishopCheckSquares;
    Bitboard rookChecks = evalData.attackedBy[us][PieceType::ROOK] & rookCheckSquares;
    Bitboard queenChecks = evalData.attackedBy[us][PieceType::QUEEN] & (bishopCheckSquares | rookCheckSquares);

    Bitboard weak = ~evalData.attacked[them] | (~evalData.attackedBy2[them] & evalData.attackedBy[them][PieceType::KING]);
    Bitboard safe = ~board.allPieces() & ~evalData.attacked[them] | (weak & evalData.attackedBy2[us]);

    TRACE_ADD(safeKnightCheck, (knightChecks & safe).popcount());
    TRACE_ADD(safeBishopCheck, (bishopChecks & safe).popcount());
    TRACE_ADD(safeRookCheck, (rookChecks & safe).popcount());
    TRACE_ADD(safeQueenCheck, (queenChecks & safe).popcount());

    TRACE_ADD(unsafeKnightCheck, (knightChecks & ~safe).popcount());
    TRACE_ADD(unsafeBishopCheck, (bishopChecks & ~safe).popcount());
    TRACE_ADD(unsafeRookCheck, (rookChecks & ~safe).popcount());
    TRACE_ADD(unsafeQueenCheck, (queenChecks & ~safe).popcount());

    int attackCount = std::min(evalData.attackCount[us], 13);
    TRACE_INC(kingAttacks[attackCount]);

    int weakSquares = std::min((evalData.kingRing[them] & weak).popcount(), 8u);
    TRACE_INC(weakKingRing[weakSquares]);

    eval += SAFE_KNIGHT_CHECK * (knightChecks & safe).popcount();
    eval += SAFE_BISHOP_CHECK * (bishopChecks & safe).popcount();
    eval += SAFE_ROOK_CHECK * (rookChecks & safe).popcount();
    eval += SAFE_QUEEN_CHECK * (queenChecks & safe).popcount();

    eval += UNSAFE_KNIGHT_CHECK * (knightChecks & ~safe).popcount();
    eval += UNSAFE_BISHOP_CHECK * (bishopChecks & ~safe).popcount();
    eval += UNSAFE_ROOK_CHECK * (rookChecks & ~safe).popcount();
    eval += UNSAFE_QUEEN_CHECK * (queenChecks & ~safe).popcount();

    eval += evalData.attackWeight[us];
    eval += KING_ATTACKS[attackCount];

    eval += WEAK_KING_RING[weakSquares];

    return eval;
}

PackedScore evaluateComplexity(const Board& board, const PawnStructure& pawnStructure, PackedScore eval, Trace& trace)
{
    constexpr Bitboard KING_SIDE = FILE_A_BB | FILE_B_BB | FILE_C_BB | FILE_D_BB;
    constexpr Bitboard QUEEN_SIDE = ~KING_SIDE;
    Bitboard pawns = board.pieces(PieceType::PAWN);
    bool pawnsBothSides = (pawns & KING_SIDE).any() && (pawns & QUEEN_SIDE).any();
    bool pawnEndgame = board.allPieces() == (pawns | board.pieces(PieceType::KING));

    trace.complexityPawns[Color::WHITE] += pawns.popcount();
    trace.complexityPassers[Color::WHITE] += pawnStructure.passedPawns.popcount();
    trace.complexityPawnsBothSides[Color::WHITE] += pawnsBothSides;
    trace.complexityPawnEndgame[Color::WHITE] += pawnEndgame;
    trace.complexityOffset[Color::WHITE] = 1;

    PackedScore complexity =
        COMPLEXITY_PAWNS * pawns.popcount() +
        COMPLEXITY_PASSERS * pawnStructure.passedPawns.popcount() +
        COMPLEXITY_PAWNS_BOTH_SIDES * pawnsBothSides +
        COMPLEXITY_PAWN_ENDGAME * pawnEndgame +
        COMPLEXITY_OFFSET;

    int egSign = (eval.eg() > 0) - (eval.eg() < 0);

    int egComplexity = std::max(complexity.eg(), -std::abs(eval.eg()));

    return PackedScore(0, egSign * egComplexity);
}

void initEvalData(const Board& board, EvalData& evalData, const PawnStructure& pawnStructure)
{
    Bitboard whitePawns = board.pieces(Color::WHITE, PieceType::PAWN);
    Bitboard blackPawns = board.pieces(Color::BLACK, PieceType::PAWN);
    Square whiteKing = board.kingSq(Color::WHITE);
    Square blackKing = board.kingSq(Color::BLACK);

    evalData.mobilityArea[Color::WHITE] = ~pawnStructure.pawnAttacks[Color::BLACK];
    evalData.attacked[Color::WHITE] = evalData.attackedBy[Color::WHITE][PieceType::PAWN] = pawnStructure.pawnAttacks[Color::WHITE];

    Bitboard whiteKingAtks = attacks::kingAttacks(whiteKing);
    evalData.attackedBy[Color::WHITE][PieceType::KING] = whiteKingAtks;
    evalData.attackedBy2[Color::WHITE] = evalData.attacked[Color::WHITE] & whiteKingAtks;
    evalData.attacked[Color::WHITE] |= whiteKingAtks;
    evalData.kingRing[Color::WHITE] = (whiteKingAtks | whiteKingAtks.north()) & ~Bitboard::fromSquare(whiteKing);
    if ((Bitboard::fromSquare(whiteKing) & FILE_H_BB).any())
        evalData.kingRing[Color::WHITE] |= evalData.kingRing[Color::WHITE].west();
    if ((Bitboard::fromSquare(whiteKing) & FILE_A_BB).any())
        evalData.kingRing[Color::WHITE] |= evalData.kingRing[Color::WHITE].east();

    evalData.mobilityArea[Color::BLACK] = ~pawnStructure.pawnAttacks[Color::WHITE];
    evalData.attacked[Color::BLACK] = evalData.attackedBy[Color::BLACK][PieceType::PAWN] = pawnStructure.pawnAttacks[Color::BLACK];

    Bitboard blackKingAtks = attacks::kingAttacks(blackKing);
    evalData.attackedBy[Color::BLACK][PieceType::KING] = blackKingAtks;
    evalData.attackedBy2[Color::BLACK] = evalData.attacked[Color::BLACK] & blackKingAtks;
    evalData.attacked[Color::BLACK] |= blackKingAtks;
    evalData.kingRing[Color::BLACK] = (blackKingAtks | blackKingAtks.south()) & ~Bitboard::fromSquare(blackKing);
    if ((Bitboard::fromSquare(blackKing) & FILE_H_BB).any())
        evalData.kingRing[Color::BLACK] |= evalData.kingRing[Color::BLACK].west();
    if ((Bitboard::fromSquare(blackKing) & FILE_A_BB).any())
        evalData.kingRing[Color::BLACK] |= evalData.kingRing[Color::BLACK].east();
}

PackedScore evaluatePsqt(const Board& board, Trace& trace)
{
    PackedScore eval{0, 0};
    for (Color c : {Color::WHITE, Color::BLACK})
    {
        bool mirror = board.kingSq(c).file() >= FILE_E;
        for (PieceType pt : {PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN, PieceType::KING})
        {
            Bitboard pieces = board.pieces(c, pt);
            while (pieces.any())
            {
                Square sq = pieces.poplsb();
                // hack for now
                int x = 0;
                if (c == Color::WHITE)
                    x ^= 56;
                if (mirror)
                    x ^= 7;
                trace.psqt[static_cast<int>(pt)][sq.value() ^ x][c]++;
                PackedScore d = MATERIAL[static_cast<int>(pt)] + PSQT[static_cast<int>(pt)][sq.value() ^ x];
                if (c == Color::WHITE)
                    eval += d;
                else
                    eval -= d;
            }
        }
    }

    return eval;
}

double evaluateScale(const Board& board, PackedScore eval)
{
    Color strongSide = eval.eg() > 0 ? Color::WHITE : Color::BLACK;

    int strongPawns = board.pieces(strongSide, PieceType::PAWN).popcount();

    return 80 + strongPawns * 7;
}

Trace getTrace(const Board& board)
{
    Trace trace = {};

    PackedScore eval = evaluatePsqt(board, trace);

    PawnStructure pawnStructure(board);

    EvalData evalData = {};
    initEvalData(board, evalData, pawnStructure);


    eval += evaluatePawns(board, pawnStructure, trace);

    eval += evaluateKnightOutposts<Color::WHITE>(board, pawnStructure, trace) - evaluateKnightOutposts<Color::BLACK>(board, pawnStructure, trace);
    eval += evaluateBishopPawns<Color::WHITE>(board, trace) - evaluateBishopPawns<Color::BLACK>(board, trace);
    eval += evaluateRookOpen<Color::WHITE>(board, trace) - evaluateRookOpen<Color::BLACK>(board, trace);
    eval += evaluateMinorBehindPawn<Color::WHITE>(board, trace) - evaluateMinorBehindPawn<Color::BLACK>(board, trace);

    eval += evaluatePieces<Color::WHITE, PieceType::KNIGHT>(board, evalData, trace) - evaluatePieces<Color::BLACK, PieceType::KNIGHT>(board, evalData, trace);
    eval += evaluatePieces<Color::WHITE, PieceType::BISHOP>(board, evalData, trace) - evaluatePieces<Color::BLACK, PieceType::BISHOP>(board, evalData, trace);
    eval += evaluatePieces<Color::WHITE, PieceType::ROOK>(board, evalData, trace) - evaluatePieces<Color::BLACK, PieceType::ROOK>(board, evalData, trace);
    eval += evaluatePieces<Color::WHITE, PieceType::QUEEN>(board, evalData, trace) - evaluatePieces<Color::BLACK, PieceType::QUEEN>(board, evalData, trace);

    eval += evaluateKings<Color::WHITE>(board, evalData, trace) - evaluateKings<Color::BLACK>(board, evalData, trace);
    eval += evaluatePassedPawns<Color::WHITE>(board, pawnStructure, evalData, trace) - evaluatePassedPawns<Color::BLACK>(board, pawnStructure, evalData, trace);
    eval += evaluateThreats<Color::WHITE>(board, evalData, trace) - evaluateThreats<Color::BLACK>(board, evalData, trace);

    eval += evaluateComplexity(board, pawnStructure, eval, trace);

    trace.tempo[board.sideToMove()]++;

    trace.egScale = evaluateScale(board, eval) / 128.0;

    eval += (board.sideToMove() == Color::WHITE ? TEMPO : -TEMPO);

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

std::tuple<size_t, size_t, double> EvalFn::getCoefficients(const Board& board)
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

    addCoefficientArray(trace.isolatedPawn);
    addCoefficientArray(trace.doubledPawn);
    addCoefficientArray(trace.backwardsPawn);
    addCoefficientArray(trace.pawnPhalanx);
    addCoefficientArray(trace.defendedPawn);

    addCoefficientArray3D(trace.passedPawn);
    addCoefficientArray(trace.ourPasserProximity);
    addCoefficientArray(trace.theirPasserProximity);

    addCoefficientArray3D(trace.pawnStorm);
    addCoefficientArray2D(trace.pawnShield);
    addCoefficient(trace.safeKnightCheck);
    addCoefficient(trace.safeBishopCheck);
    addCoefficient(trace.safeRookCheck);
    addCoefficient(trace.safeQueenCheck);
    addCoefficient(trace.unsafeKnightCheck);
    addCoefficient(trace.unsafeBishopCheck);
    addCoefficient(trace.unsafeRookCheck);
    addCoefficient(trace.unsafeQueenCheck);
    addCoefficientArray(trace.kingAttackerWeight);
    addCoefficientArray(trace.kingAttacks);
    addCoefficientArray(trace.weakKingRing);

    addCoefficient(trace.minorBehindPawn);
    addCoefficient(trace.knightOutpost);
    addCoefficientArray(trace.bishopPawns);
    addCoefficient(trace.bishopPair);
    addCoefficientArray(trace.openRook);

    addCoefficient(trace.tempo);

    addCoefficient(trace.complexityPawns);
    addCoefficient(trace.complexityPassers);
    addCoefficient(trace.complexityPawnsBothSides);
    addCoefficient(trace.complexityPawnEndgame);
    addCoefficient(trace.complexityOffset);

    return {pos, m_Coefficients.size(), trace.egScale};
}

template<typename T>
void addEvalParam(EvalParams& params, const T& t, ParamType type)
{
    params.push_back({type, static_cast<double>(t.mg()), static_cast<double>(t.eg())});
}

template<typename T>
void addEvalParamArray(EvalParams& params, const T& t, ParamType type)
{
    for (auto param : t)
        addEvalParam(params, param, type);
}

template<typename T>
void addEvalParamArray2D(EvalParams& params, const T& t, ParamType type)
{
    for (auto& array : t)
        addEvalParamArray(params, array, type);
}

template<typename T>
void addEvalParamArray3D(EvalParams& params, const T& t, ParamType type)
{
    for (auto& array : t)
        addEvalParamArray2D(params, array, type);
}

EvalParams EvalFn::getInitialParams()
{
    EvalParams params;
    addEvalParamArray2D(params, PSQT, ParamType::NORMAL);
    for (int i = 0; i < 6; i++)
        for (int j = (i == 0 ? 8 : 0); j < (i == 0 ? 56 : 64); j++)
        {
            params[i * 64 + j].mg += MATERIAL[i].mg();
            params[i * 64 + j].eg += MATERIAL[i].eg();
        }
    addEvalParamArray2D(params, MOBILITY, ParamType::NORMAL);

    addEvalParamArray(params, THREAT_BY_PAWN, ParamType::NORMAL);
    addEvalParamArray2D(params, THREAT_BY_KNIGHT, ParamType::NORMAL);
    addEvalParamArray2D(params, THREAT_BY_BISHOP, ParamType::NORMAL);
    addEvalParamArray2D(params, THREAT_BY_ROOK, ParamType::NORMAL);
    addEvalParamArray2D(params, THREAT_BY_QUEEN, ParamType::NORMAL);
    addEvalParamArray(params, THREAT_BY_KING, ParamType::NORMAL);
    addEvalParam(params, PUSH_THREAT, ParamType::NORMAL);

    addEvalParamArray(params, ISOLATED_PAWN, ParamType::NORMAL);
    addEvalParamArray(params, DOUBLED_PAWN, ParamType::NORMAL);
    addEvalParamArray(params, BACKWARDS_PAWN, ParamType::NORMAL);
    addEvalParamArray(params, PAWN_PHALANX, ParamType::NORMAL);
    addEvalParamArray(params, DEFENDED_PAWN, ParamType::NORMAL);

    addEvalParamArray3D(params, PASSED_PAWN, ParamType::NORMAL);
    addEvalParamArray(params, OUR_PASSER_PROXIMITY, ParamType::NORMAL);
    addEvalParamArray(params, THEIR_PASSER_PROXIMITY, ParamType::NORMAL);

    addEvalParamArray3D(params, PAWN_STORM, ParamType::NORMAL);
    addEvalParamArray2D(params, PAWN_SHIELD, ParamType::NORMAL);
    addEvalParam(params, SAFE_KNIGHT_CHECK, ParamType::NORMAL);
    addEvalParam(params, SAFE_BISHOP_CHECK, ParamType::NORMAL);
    addEvalParam(params, SAFE_ROOK_CHECK, ParamType::NORMAL);
    addEvalParam(params, SAFE_QUEEN_CHECK, ParamType::NORMAL);
    addEvalParam(params, UNSAFE_KNIGHT_CHECK, ParamType::NORMAL);
    addEvalParam(params, UNSAFE_BISHOP_CHECK, ParamType::NORMAL);
    addEvalParam(params, UNSAFE_ROOK_CHECK, ParamType::NORMAL);
    addEvalParam(params, UNSAFE_QUEEN_CHECK, ParamType::NORMAL);
    addEvalParamArray(params, KING_ATTACKER_WEIGHT, ParamType::NORMAL);
    addEvalParamArray(params, KING_ATTACKS, ParamType::NORMAL);
    addEvalParamArray(params, WEAK_KING_RING, ParamType::NORMAL);

    addEvalParam(params, MINOR_BEHIND_PAWN, ParamType::NORMAL);
    addEvalParam(params, KNIGHT_OUTPOST, ParamType::NORMAL);
    addEvalParamArray(params, BISHOP_PAWNS, ParamType::NORMAL);
    addEvalParam(params, BISHOP_PAIR, ParamType::NORMAL);
    addEvalParamArray(params, ROOK_OPEN, ParamType::NORMAL);
    addEvalParam(params, TEMPO, ParamType::NORMAL);

    addEvalParam(params, COMPLEXITY_PAWNS, ParamType::COMPLEXITY);
    addEvalParam(params, COMPLEXITY_PASSERS, ParamType::COMPLEXITY);
    addEvalParam(params, COMPLEXITY_PAWNS_BOTH_SIDES, ParamType::COMPLEXITY);
    addEvalParam(params, COMPLEXITY_PAWN_ENDGAME, ParamType::COMPLEXITY);
    addEvalParam(params, COMPLEXITY_OFFSET, ParamType::COMPLEXITY);

    return params;
}

EvalParams EvalFn::getMaterialParams()
{
    EvalParams params = getInitialParams();
    for (auto& param : params)
        param.mg = param.eg = 0;

    for (int i = 0; i < 6; i++)
        for (int j = (i == 0 ? 8 : 0); j < (i == 0 ? 56 : 64); j++)
        {
            params[i * 64 + j].mg += MATERIAL[i].mg();
            params[i * 64 + j].eg += MATERIAL[i].eg();
        }
    return params;
}

EvalParams EvalFn::getKParams()
{
    constexpr PackedScore K_MATERIAL[6] = {
        {67, 98}, {311, 409}, {330, 419}, {426, 746}, {860, 1403}, {0, 0}
    };

    EvalParams params = getInitialParams();
    for (auto& param : params)
        param.mg = param.eg = 0;

    for (int i = 0; i < 6; i++)
        for (int j = (i == 0 ? 8 : 0); j < (i == 0 ? 56 : 64); j++)
        {
            params[i * 64 + j].mg += K_MATERIAL[i].mg();
            params[i * 64 + j].eg += K_MATERIAL[i].eg();
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
        state.ss << "S(" << std::setw(ALIGN_SIZE) << static_cast<int>(param.mg) << ", " << std::setw(ALIGN_SIZE) << static_cast<int>(param.eg) << ')';
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
void printArray2D(PrintState& state, int outerLen, int innerLen, bool indent = false)
{
    state.ss << "{\n";
    for (int i = 0; i < outerLen; i++)
    {
        state.ss << '\t';
        if (indent)
            state.ss << '\t';
        printArray<ALIGN_SIZE>(state, innerLen);
        if (i != outerLen - 1)
            state.ss << ',';
        state.ss << '\n';
    }
    if (indent)
        state.ss << '\t';
    state.ss << "}";
}

template<int ALIGN_SIZE>
void printArray3D(PrintState& state, int len1, int len2, int len3)
{
    state.ss << "{\n";
    for (int i = 0; i < len1; i++)
    {
        state.ss << '\t';
        printArray2D<ALIGN_SIZE>(state, len2, len3, true);
        if (i != len1 - 1)
            state.ss << ',';
        state.ss << '\n';
    }
    state.ss << "}";
}

template<int ALIGN_SIZE>
void printRestParams(PrintState& state)
{
    state.ss << "constexpr PackedScore MOBILITY[4][28] = ";
    printArray2D<ALIGN_SIZE>(state, 4, 28);
    state.ss << ";\n";

    state.ss << '\n';

    state.ss << "constexpr PackedScore THREAT_BY_PAWN[6] = ";
    printArray<ALIGN_SIZE>(state, 6);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore THREAT_BY_KNIGHT[2][6] = ";
    printArray2D<ALIGN_SIZE>(state, 2, 6);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore THREAT_BY_BISHOP[2][6] = ";
    printArray2D<ALIGN_SIZE>(state, 2, 6);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore THREAT_BY_ROOK[2][6] = ";
    printArray2D<ALIGN_SIZE>(state, 2, 6);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore THREAT_BY_QUEEN[2][6] = ";
    printArray2D<ALIGN_SIZE>(state, 2, 6);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore THREAT_BY_KING[6] = ";
    printArray<ALIGN_SIZE>(state, 6);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore PUSH_THREAT = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << '\n';

    state.ss << "constexpr PackedScore ISOLATED_PAWN[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore DOUBLED_PAWN[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore BACKWARDS_PAWN[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore PAWN_PHALANX[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore DEFENDED_PAWN[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << '\n';

    state.ss << "constexpr PackedScore PASSED_PAWN[2][2][8] = ";
    printArray3D<ALIGN_SIZE>(state, 2, 2, 8);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore OUR_PASSER_PROXIMITY[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore THEIR_PASSER_PROXIMITY[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << '\n';

    state.ss << "constexpr PackedScore PAWN_STORM[2][4][8] = ";
    printArray3D<ALIGN_SIZE>(state, 2, 4, 8);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore PAWN_SHIELD[4][8] = ";
    printArray2D<ALIGN_SIZE>(state, 4, 8);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore SAFE_KNIGHT_CHECK = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore SAFE_BISHOP_CHECK = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore SAFE_ROOK_CHECK = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore SAFE_QUEEN_CHECK = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore UNSAFE_KNIGHT_CHECK = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore UNSAFE_BISHOP_CHECK = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore UNSAFE_ROOK_CHECK = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore UNSAFE_QUEEN_CHECK = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore KING_ATTACKER_WEIGHT[4] = ";
    printArray<ALIGN_SIZE>(state, 4);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore KING_ATTACKS[14] = ";
    printArray<ALIGN_SIZE>(state, 14);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore WEAK_KING_RING[9] = ";
    printArray<ALIGN_SIZE>(state, 9);
    state.ss << ";\n";

    state.ss << '\n';

    state.ss << "constexpr PackedScore MINOR_BEHIND_PAWN = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore KNIGHT_OUTPOST = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore BISHOP_PAWNS[7] = ";
    printArray<ALIGN_SIZE>(state, 7);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore BISHOP_PAIR = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore ROOK_OPEN[2] = ";
    printArray<ALIGN_SIZE>(state, 2);
    state.ss << ";\n";

    state.ss << '\n';

    state.ss << "constexpr PackedScore TEMPO = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << '\n';

    state.ss << "constexpr PackedScore COMPLEXITY_PAWNS = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore COMPLEXITY_PASSERS = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore COMPLEXITY_PAWNS_BOTH_SIDES = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore COMPLEXITY_PAWN_ENDGAME = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore COMPLEXITY_OFFSET = ";
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

std::array<int, 2> avgValue(EvalParams& params, int offset, int size)
{
    std::array<int, 2> avg = {};
    for (int i = offset; i < offset + size; i++)
    {
        avg[0] += static_cast<int>(params[i].mg);
        avg[1] += static_cast<int>(params[i].eg);
    }
    avg[0] /= size;
    avg[1] /= size;
    return avg;
}

void rebalance(std::array<int, 2> factor, std::array<int, 2>& materialValue, EvalParams& params, int offset, int size)
{
    materialValue[0] += factor[0];
    materialValue[1] += factor[1];

    for (int i = offset; i < offset + size; i++)
    {
        params[i].mg -= static_cast<double>(factor[0]);
        params[i].eg -= static_cast<double>(factor[1]);
    }
}

EvalParams extractMaterial(const EvalParams& params)
{
    EvalParams rebalanced = params;
    for (auto& elem : rebalanced)
    {
        elem.mg = std::round(elem.mg);
        elem.eg = std::round(elem.eg);
    }

    std::array<std::array<int, 2>, 6> material = {};

    // psqts
    for (int pce = 0; pce < 6; pce++)
    {
        if (pce == 0)
        {
            auto avg = avgValue(rebalanced, TRACE_OFFSET(psqt[0]) + 24, TRACE_SIZE(psqt[0]) - 32);
            rebalance(avg, material[pce], rebalanced, TRACE_OFFSET(psqt[0]) + 8, TRACE_SIZE(psqt[0]) - 16);
        }
        else
        {
            auto avg = avgValue(rebalanced, TRACE_OFFSET(psqt[pce]), TRACE_SIZE(psqt[pce]));
            rebalance(avg, material[pce], rebalanced, TRACE_OFFSET(psqt[pce]), TRACE_SIZE(psqt[pce]));
        }
    }

    // mobility
    rebalance(avgValue(rebalanced, TRACE_OFFSET(mobility[0]), 9), material[1], rebalanced, TRACE_OFFSET(mobility[0]), 9);
    rebalance(avgValue(rebalanced, TRACE_OFFSET(mobility[1]), 14), material[2], rebalanced, TRACE_OFFSET(mobility[1]), 14);
    rebalance(avgValue(rebalanced, TRACE_OFFSET(mobility[2]), 15), material[3], rebalanced, TRACE_OFFSET(mobility[2]), 15);
    rebalance(avgValue(rebalanced, TRACE_OFFSET(mobility[3]), 28), material[4], rebalanced, TRACE_OFFSET(mobility[3]), 28);

    // king attacks
    rebalance(avgValue(rebalanced, TRACE_OFFSET(kingAttacks), TRACE_SIZE(kingAttacks)), material[5], rebalanced, TRACE_OFFSET(kingAttacks), TRACE_SIZE(kingAttacks));

    // bishop pawns
    rebalance(avgValue(rebalanced, TRACE_OFFSET(bishopPawns), TRACE_SIZE(bishopPawns)), material[2], rebalanced, TRACE_OFFSET(bishopPawns), TRACE_SIZE(bishopPawns));

    EvalParams extracted;
    for (int i = 0; i < 5; i++)
    {
        extracted.push_back(EvalParam{ParamType::NORMAL, static_cast<double>(material[i][0]), static_cast<double>(material[i][1])});
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
