#include "eval_fn.h"
#include "eval_constants.h"
#include "sirius/attacks.h"

#include <iomanip>
#include <sstream>

using TraceElem = ColorArray<i32>;

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
    TraceElem knightHitQueen;
    TraceElem bishopHitQueen;
    TraceElem rookHitQueen;
    TraceElem pushThreat;
    TraceElem restrictedSquares;

    TraceElem isolatedPawn[4];
    TraceElem isolatedExposed;
    TraceElem doubledPawn[4];
    TraceElem backwardsPawn[8];
    TraceElem backwardsExposed;
    TraceElem pawnPhalanx[8];
    TraceElem defendedPawn[8];
    TraceElem candidatePasser[2][8];

    TraceElem passedPawn[2][2][8];
    TraceElem ourPasserProximity[8];
    TraceElem theirPasserProximity[8];
    TraceElem passerDefendedPush[8];
    TraceElem passerSliderBehind[8];

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
    TraceElem queenlessAttack;
    TraceElem kingAttackerWeight[4];
    TraceElem kingAttacks;
    TraceElem weakKingRing;
    TraceElem kingFlankAttacks[2];
    TraceElem kingFlankDefenses[2];
    TraceElem safetyPinned[5][3];
    TraceElem safetyDiscovered[6][3];
    TraceElem safetyOffset;

    TraceElem minorBehindPawn;
    TraceElem knightOutpost;
    TraceElem bishopPawns[7];
    TraceElem bishopPair;
    TraceElem longDiagBishop;
    TraceElem openRook[2];

    TraceElem tempo;

    TraceElem complexityPawns;
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
    ColorArray<ScorePair> attackWeight;
    ColorArray<i32> attackCount;
    ColorArray<Bitboard> kingFlank;
};

using enum Color;
using enum PieceType;

struct PawnStructure
{
    PawnStructure() = default;
    PawnStructure(const Board& board)
    {
        Bitboard wpawns = board.pieces(WHITE, PAWN);
        Bitboard bpawns = board.pieces(BLACK, PAWN);
        pawnAttacks[WHITE] = attacks::pawnAttacks<WHITE>(wpawns);
        pawnAttackSpans[WHITE] = attacks::fillUp<WHITE>(pawnAttacks[WHITE]);
        passedPawns = Bitboard(0);

        pawnAttacks[BLACK] = attacks::pawnAttacks<BLACK>(bpawns);
        pawnAttackSpans[BLACK] = attacks::fillUp<BLACK>(pawnAttacks[BLACK]);
        passedPawns = Bitboard(0);
    }

    ColorArray<Bitboard> pawnAttacks;
    ColorArray<Bitboard> pawnAttackSpans;
    Bitboard passedPawns;
};

template<Color us>
ScorePair evaluateKnightOutposts(const Board& board, const PawnStructure& pawnStructure, Trace& trace)
{
    constexpr Color them = ~us;
    Bitboard outpostRanks = RANK_4_BB | RANK_5_BB | (us == WHITE ? RANK_6_BB : RANK_3_BB);
    Bitboard outposts =
        outpostRanks & ~pawnStructure.pawnAttackSpans[them] & pawnStructure.pawnAttacks[us];
    TRACE_ADD(knightOutpost, (board.pieces(us, KNIGHT) & outposts).popcount());
    return KNIGHT_OUTPOST * (board.pieces(us, KNIGHT) & outposts).popcount();
}

template<Color us>
ScorePair evaluateBishopPawns(const Board& board, Trace& trace)
{
    Bitboard bishops = board.pieces(us, BISHOP);

    ScorePair eval = ScorePair(0, 0);
    while (bishops.any())
    {
        Square sq = bishops.poplsb();
        bool lightSquare = LIGHT_SQUARES_BB.has(sq);
        Bitboard sameColorPawns =
            board.pieces(us, PAWN) & (lightSquare ? LIGHT_SQUARES_BB : DARK_SQUARES_BB);
        TRACE_INC(bishopPawns[std::min(sameColorPawns.popcount(), 6u)]);
        eval += BISHOP_PAWNS[std::min(sameColorPawns.popcount(), 6u)];
    }
    return eval;
}

template<Color us>
ScorePair evaluateRookOpen(const Board& board, Trace& trace)
{
    constexpr Color them = ~us;
    Bitboard ourPawns = board.pieces(us, PAWN);
    Bitboard theirPawns = board.pieces(them, PAWN);
    Bitboard rooks = board.pieces(us, ROOK);

    ScorePair eval = ScorePair(0, 0);
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
ScorePair evaluateMinorBehindPawn(const Board& board, Trace& trace)
{
    constexpr Color them = ~us;

    Bitboard pawns = board.pieces(PAWN);
    Bitboard minors = board.pieces(us, KNIGHT) | board.pieces(us, BISHOP);

    Bitboard shielded = minors & attacks::pawnPushes<them>(pawns);
    TRACE_ADD(minorBehindPawn, shielded.popcount());
    return MINOR_BEHIND_PAWN * shielded.popcount();
}

template<Color us, PieceType piece>
ScorePair evaluatePieces(const Board& board, EvalData& evalData, Trace& trace)
{
    constexpr Color them = ~us;
    constexpr Bitboard CENTER_SQUARES = (RANK_4_BB | RANK_5_BB) & (FILE_D_BB | FILE_E_BB);

    ScorePair eval = ScorePair(0, 0);
    Bitboard pieces = board.pieces(us, piece);
    if constexpr (piece == BISHOP)
        if (pieces.multiple())
        {
            eval += BISHOP_PAIR;
            TRACE_INC(bishopPair);
        }

    Bitboard occupancy = board.allPieces();
    if constexpr (piece == BISHOP)
        occupancy ^= board.pieces(us, BISHOP) | board.pieces(us, QUEEN);
    else if constexpr (piece == ROOK)
        occupancy ^= board.pieces(us, ROOK) | board.pieces(us, QUEEN);
    else if constexpr (piece == QUEEN)
        occupancy ^= board.pieces(us, BISHOP) | board.pieces(us, ROOK);

    while (pieces.any())
    {
        Square sq = pieces.poplsb();
        Bitboard attacks = attacks::pieceAttacks<piece>(sq, occupancy);
        if (board.checkBlockers(us).has(sq))
            attacks &= attacks::inBetweenSquares(sq, board.pieces(us, KING).lsb());

        evalData.attackedBy[us][piece] |= attacks;
        evalData.attackedBy2[us] |= evalData.attacked[us] & attacks;
        evalData.attacked[us] |= attacks;

        eval += MOBILITY[static_cast<i32>(piece) - static_cast<i32>(KNIGHT)]
                        [(attacks & evalData.mobilityArea[us]).popcount()];
        TRACE_INC(mobility[static_cast<i32>(piece) - static_cast<i32>(KNIGHT)]
                          [(attacks & evalData.mobilityArea[us]).popcount()]);

        if (Bitboard kingRingAtks = evalData.kingRing[them] & attacks; kingRingAtks.any())
        {
            evalData.attackWeight[us] +=
                KING_ATTACKER_WEIGHT[static_cast<i32>(piece) - static_cast<i32>(KNIGHT)];
            TRACE_INC(kingAttackerWeight[static_cast<i32>(piece) - static_cast<i32>(KNIGHT)]);
            evalData.attackCount[us] += kingRingAtks.popcount();
        }

        if (piece == BISHOP && (attacks & CENTER_SQUARES).multiple())
        {
            eval += LONG_DIAG_BISHOP;
            TRACE_INC(longDiagBishop);
        }
    }

    return eval;
}

template<Color us>
ScorePair evaluatePawns(const Board& board, PawnStructure& pawnStructure, Trace& trace)
{
    constexpr Color them = ~us;
    Bitboard ourPawns = board.pieces(us, PAWN);
    Bitboard theirPawns = board.pieces(them, PAWN);

    ScorePair eval = ScorePair(0, 0);

    Bitboard pawns = ourPawns;
    while (pawns.any())
    {
        Square sq = pawns.poplsb();
        Square push = sq + attacks::pawnPushOffset<us>();
        Bitboard attacks = attacks::pawnAttacks(us, sq);
        Bitboard neighbors = attacks::isolatedPawnMask(sq) & ourPawns;
        Bitboard support = attacks::passedPawnMask(them, push) & neighbors;
        Bitboard threats = attacks & theirPawns;
        Bitboard pushThreats = attacks::pawnPushes<us>(attacks) & theirPawns;
        Bitboard defenders = attacks::pawnAttacks(them, sq) & ourPawns;
        Bitboard phalanx = attacks::pawnAttacks(them, push) & ourPawns;
        Bitboard stoppers = attacks::passedPawnMask(us, sq) & theirPawns;

        bool exposed = (stoppers & Bitboard::fileBB(sq.file())).empty();
        bool blocked = theirPawns.has(push);
        bool doubled = ourPawns.has(push);
        bool isolated = neighbors.empty();
        bool backwards = (blocked || pushThreats.any()) && support.empty();

        if (stoppers.empty())
            pawnStructure.passedPawns |= Bitboard::fromSquare(sq);
        else if (stoppers == (pushThreats | threats) && phalanx.popcount() >= pushThreats.popcount())
        {
            bool defended = defenders.popcount() >= threats.popcount();
            eval += CANDIDATE_PASSER[defended][sq.relativeRank<us>()];
            TRACE_INC(candidatePasser[defended][sq.relativeRank<us>()]);
        }

        if (doubled && threats.empty())
        {
            eval += DOUBLED_PAWN[std::min(sq.file(), sq.file() ^ 7)];
            TRACE_INC(doubledPawn[std::min(sq.file(), sq.file() ^ 7)]);
        }

        if (threats.empty() && isolated)
        {
            eval += ISOLATED_PAWN[std::min(sq.file(), sq.file() ^ 7)] + exposed * ISOLATED_EXPOSED;
            TRACE_INC(isolatedPawn[std::min(sq.file(), sq.file() ^ 7)]);
            TRACE_ADD(isolatedExposed, exposed);
        }
        else if (backwards)
        {
            eval += BACKWARDS_PAWN[sq.relativeRank<us>()] + exposed * BACKWARDS_EXPOSED;
            TRACE_INC(backwardsPawn[sq.relativeRank<us>()]);
            TRACE_ADD(backwardsExposed, exposed);
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
ScorePair evaluatePassedPawns(
    const Board& board, const PawnStructure& pawnStructure, const EvalData& evalData, Trace& trace)
{
    constexpr Color them = ~us;
    Square ourKing = board.kingSq(us);
    Square theirKing = board.kingSq(them);

    Bitboard passers = pawnStructure.passedPawns & board.pieces(us);

    ScorePair eval = ScorePair(0, 0);

    while (passers.any())
    {
        Square passer = passers.poplsb();
        i32 rank = passer.relativeRank<us>();
        if (rank >= RANK_4)
        {
            Square pushSq = passer + attacks::pawnPushOffset<us>();

            bool blocked = board.pieceAt(pushSq) != Piece::NONE;
            bool controlled = evalData.attacked[them].has(pushSq);
            eval += PASSED_PAWN[blocked][controlled][rank];
            TRACE_INC(passedPawn[blocked][controlled][rank]);

            eval += OUR_PASSER_PROXIMITY[Square::chebyshev(ourKing, pushSq)];
            eval += THEIR_PASSER_PROXIMITY[Square::chebyshev(theirKing, pushSq)];
            TRACE_INC(ourPasserProximity[Square::chebyshev(ourKing, pushSq)]);
            TRACE_INC(theirPasserProximity[Square::chebyshev(theirKing, pushSq)]);

            if (evalData.attacked[us].has(pushSq))
            {
                eval += PASSER_DEFENDED_PUSH[rank];
                TRACE_INC(passerDefendedPush[rank]);
            }

            Bitboard squaresBehind =
                attacks::passedPawnMask(them, passer) & Bitboard::fileBB(passer.file());
            Bitboard verticalSliders =
                board.pieces(them, PieceType::ROOK) | board.pieces(them, PieceType::QUEEN);
            if ((squaresBehind & verticalSliders).any())
            {
                eval += PASSER_SLIDER_BEHIND[rank];
                TRACE_INC(passerSliderBehind[rank]);
            }
        }
    }

    return eval;
}

ScorePair evaluatePawns(const Board& board, PawnStructure& pawnStructure, Trace& trace)
{
    return evaluatePawns<WHITE>(board, pawnStructure, trace)
        - evaluatePawns<BLACK>(board, pawnStructure, trace);
}

template<Color us>
ScorePair evaluateThreats(const Board& board, const EvalData& evalData, Trace& trace)
{
    constexpr Color them = ~us;

    ScorePair eval = ScorePair(0, 0);

    Bitboard defendedBB = evalData.attackedBy2[them] | evalData.attackedBy[them][PAWN]
        | (evalData.attacked[them] & ~evalData.attackedBy2[us]);

    Bitboard pawnThreats = evalData.attackedBy[us][PAWN] & board.pieces(them);
    while (pawnThreats.any())
    {
        PieceType threatened = getPieceType(board.pieceAt(pawnThreats.poplsb()));
        eval += THREAT_BY_PAWN[static_cast<i32>(threatened)];
        TRACE_INC(threatByPawn[static_cast<i32>(threatened)]);
    }

    Bitboard knightThreats = evalData.attackedBy[us][KNIGHT] & board.pieces(them);
    while (knightThreats.any())
    {
        Square threat = knightThreats.poplsb();
        PieceType threatened = getPieceType(board.pieceAt(threat));
        bool defended = defendedBB.has(threat);
        eval += THREAT_BY_KNIGHT[defended][static_cast<i32>(threatened)];
        TRACE_INC(threatByKnight[defended][static_cast<i32>(threatened)]);
    }

    Bitboard bishopThreats = evalData.attackedBy[us][BISHOP] & board.pieces(them);
    while (bishopThreats.any())
    {
        Square threat = bishopThreats.poplsb();
        PieceType threatened = getPieceType(board.pieceAt(threat));
        bool defended = defendedBB.has(threat);
        eval += THREAT_BY_BISHOP[defended][static_cast<i32>(threatened)];
        TRACE_INC(threatByBishop[defended][static_cast<i32>(threatened)]);
    }

    Bitboard rookThreats = evalData.attackedBy[us][ROOK] & board.pieces(them);
    while (rookThreats.any())
    {
        Square threat = rookThreats.poplsb();
        PieceType threatened = getPieceType(board.pieceAt(threat));
        bool defended = defendedBB.has(threat);
        eval += THREAT_BY_ROOK[defended][static_cast<i32>(threatened)];
        TRACE_INC(threatByRook[defended][static_cast<i32>(threatened)]);
    }

    // queen can xray through bishops and rooks to sometimes "attack" the king
    Bitboard queenThreats = evalData.attackedBy[us][QUEEN] & board.pieces(them) & ~board.pieces(KING);
    while (queenThreats.any())
    {
        Square threat = queenThreats.poplsb();
        PieceType threatened = getPieceType(board.pieceAt(threat));
        bool defended = defendedBB.has(threat);
        eval += THREAT_BY_QUEEN[defended][static_cast<i32>(threatened)];
        TRACE_INC(threatByQueen[defended][static_cast<i32>(threatened)]);
    }

    Bitboard kingThreats = evalData.attackedBy[us][KING] & board.pieces(them) & ~defendedBB;
    while (kingThreats.any())
    {
        PieceType threatened = getPieceType(board.pieceAt(kingThreats.poplsb()));
        eval += THREAT_BY_KING[static_cast<i32>(threatened)];
        TRACE_INC(threatByKing[static_cast<i32>(threatened)]);
    }

    Bitboard nonPawnEnemies = board.pieces(them) & ~board.pieces(PAWN);

    Bitboard safe = ~defendedBB
        | (evalData.attacked[us] & ~evalData.attackedBy[them][PAWN] & ~evalData.attackedBy2[them]);
    Bitboard pushes = attacks::pawnPushes<us>(board.pieces(us, PAWN)) & ~board.allPieces();
    pushes |= attacks::pawnPushes<us>(pushes & Bitboard::nthRank<us, RANK_3>()) & ~board.allPieces();

    Bitboard pushThreats = attacks::pawnAttacks<us>(pushes & safe) & nonPawnEnemies;
    eval += PUSH_THREAT * pushThreats.popcount();
    TRACE_ADD(pushThreat, pushThreats.popcount());

    Bitboard restriction =
        evalData.attackedBy2[us] & ~evalData.attackedBy2[them] & evalData.attacked[them];
    eval += RESTRICTED_SQUARES * restriction.popcount();
    TRACE_ADD(restrictedSquares, restriction.popcount());

    Bitboard oppQueens = board.pieces(them, QUEEN);
    if (oppQueens.one())
    {
        Square oppQueen = oppQueens.lsb();
        Bitboard knightHits = attacks::knightAttacks(oppQueen);
        Bitboard bishopHits = attacks::bishopAttacks(oppQueen, board.allPieces());
        Bitboard rookHits = attacks::rookAttacks(oppQueen, board.allPieces());

        Bitboard targets = safe & ~board.pieces(us, PAWN);

        eval += KNIGHT_HIT_QUEEN * (targets & knightHits & evalData.attackedBy[us][KNIGHT]).popcount();
        TRACE_ADD(knightHitQueen, (targets & knightHits & evalData.attackedBy[us][KNIGHT]).popcount());

        targets &= evalData.attackedBy2[us];
        eval += BISHOP_HIT_QUEEN * (targets & bishopHits & evalData.attackedBy[us][BISHOP]).popcount();
        TRACE_ADD(bishopHitQueen, (targets & bishopHits & evalData.attackedBy[us][BISHOP]).popcount());

        eval += ROOK_HIT_QUEEN * (targets & rookHits & evalData.attackedBy[us][ROOK]).popcount();
        TRACE_ADD(rookHitQueen, (targets & rookHits & evalData.attackedBy[us][ROOK]).popcount());
    }

    return eval;
}

template<Color us>
ScorePair evalKingPawnFile(u32 file, Bitboard ourPawns, Bitboard theirPawns, Trace& trace)
{
    constexpr Color them = ~us;

    ScorePair eval = ScorePair(0, 0);
    i32 edgeDist = std::min(file, 7 - file);
    {
        Bitboard filePawns = ourPawns & Bitboard::fileBB(file);
        i32 rank = 0;
        bool blocked = false;
        if (filePawns.any())
        {
            Square filePawn = us == WHITE ? filePawns.msb() : filePawns.lsb();
            rank = filePawn.relativeRank<them>();
            blocked = theirPawns.has(filePawn + attacks::pawnPushOffset<us>());
        }
        eval += PAWN_STORM[blocked][edgeDist][rank];
        TRACE_INC(pawnStorm[blocked][edgeDist][rank]);
    }
    {
        Bitboard filePawns = theirPawns & Bitboard::fileBB(file);
        i32 rank = filePawns.any()
            ? (us == WHITE ? filePawns.msb() : filePawns.lsb()).relativeRank<them>()
            : 0;
        eval += PAWN_SHIELD[edgeDist][rank];
        TRACE_INC(pawnShield[edgeDist][rank]);
    }

    return eval;
}

constexpr i32 safetyAdjustment(i32 value)
{
    return (value + std::max(value, 0) * value / 128) / 8;
}

template<Color us>
ScorePair evaluateKings(const Board& board, const EvalData& evalData, Trace& trace)
{
    constexpr Color them = ~us;
    Bitboard ourPawns = board.pieces(us, PAWN);
    Bitboard theirPawns = board.pieces(them, PAWN);

    Square theirKing = board.kingSq(them);

    ScorePair eval = ScorePair(0, 0);

    u32 leftFile = std::clamp(theirKing.file() - 1, FILE_A, FILE_F);
    u32 rightFile = std::clamp(theirKing.file() + 1, FILE_C, FILE_H);
    for (u32 file = leftFile; file <= rightFile; file++)
        eval += evalKingPawnFile<us>(file, ourPawns, theirPawns, trace);

    Bitboard rookCheckSquares = attacks::rookAttacks(theirKing, board.allPieces());
    Bitboard bishopCheckSquares = attacks::bishopAttacks(theirKing, board.allPieces());

    Bitboard knightChecks = evalData.attackedBy[us][KNIGHT] & attacks::knightAttacks(theirKing);
    Bitboard bishopChecks = evalData.attackedBy[us][BISHOP] & bishopCheckSquares;
    Bitboard rookChecks = evalData.attackedBy[us][ROOK] & rookCheckSquares;
    Bitboard queenChecks = evalData.attackedBy[us][QUEEN] & (bishopCheckSquares | rookCheckSquares);

    Bitboard weak =
        ~evalData.attacked[them] | (~evalData.attackedBy2[them] & evalData.attackedBy[them][KING]);
    Bitboard safe = ~board.pieces(us) & (~evalData.attacked[them] | (weak & evalData.attackedBy2[us]));

    eval += SAFE_KNIGHT_CHECK * (knightChecks & safe).popcount();
    eval += SAFE_BISHOP_CHECK * (bishopChecks & safe).popcount();
    eval += SAFE_ROOK_CHECK * (rookChecks & safe).popcount();
    eval += SAFE_QUEEN_CHECK * (queenChecks & safe).popcount();

    eval += UNSAFE_KNIGHT_CHECK * (knightChecks & ~safe).popcount();
    eval += UNSAFE_BISHOP_CHECK * (bishopChecks & ~safe).popcount();
    eval += UNSAFE_ROOK_CHECK * (rookChecks & ~safe).popcount();
    eval += UNSAFE_QUEEN_CHECK * (queenChecks & ~safe).popcount();

    TRACE_ADD(safeKnightCheck, (knightChecks & safe).popcount());
    TRACE_ADD(safeBishopCheck, (bishopChecks & safe).popcount());
    TRACE_ADD(safeRookCheck, (rookChecks & safe).popcount());
    TRACE_ADD(safeQueenCheck, (queenChecks & safe).popcount());

    TRACE_ADD(unsafeKnightCheck, (knightChecks & ~safe).popcount());
    TRACE_ADD(unsafeBishopCheck, (bishopChecks & ~safe).popcount());
    TRACE_ADD(unsafeRookCheck, (rookChecks & ~safe).popcount());
    TRACE_ADD(unsafeQueenCheck, (queenChecks & ~safe).popcount());

    bool queenless = board.pieces(us, QUEEN).empty();
    eval += QUEENLESS_ATTACK * queenless;
    TRACE_ADD(queenlessAttack, queenless);

    eval += evalData.attackWeight[us];

    i32 attackCount = evalData.attackCount[us];
    eval += KING_ATTACKS * attackCount;
    TRACE_ADD(kingAttacks, attackCount);

    Bitboard weakKingRing = (evalData.kingRing[them] & weak);
    i32 weakSquares = weakKingRing.popcount();
    eval += WEAK_KING_RING * weakSquares;
    TRACE_ADD(weakKingRing, weakSquares);

    Bitboard flankAttacks = evalData.kingFlank[them] & evalData.attacked[us];
    Bitboard flankAttacks2 = evalData.kingFlank[them] & evalData.attackedBy2[us];
    Bitboard flankDefenses = evalData.kingFlank[them] & evalData.attacked[them];
    Bitboard flankDefenses2 = evalData.kingFlank[them] & evalData.attackedBy2[them];

    eval += flankAttacks.popcount() * KING_FLANK_ATTACKS[0]
        + flankAttacks2.popcount() * KING_FLANK_ATTACKS[1];
    eval += flankDefenses.popcount() * KING_FLANK_DEFENSES[0]
        + flankDefenses2.popcount() * KING_FLANK_DEFENSES[1];

    TRACE_ADD(kingFlankAttacks[0], flankAttacks.popcount());
    TRACE_ADD(kingFlankAttacks[1], flankAttacks2.popcount());
    TRACE_ADD(kingFlankDefenses[0], flankDefenses.popcount());
    TRACE_ADD(kingFlankDefenses[1], flankDefenses2.popcount());

    Bitboard checkBlockers = board.checkBlockers(them);
    while (checkBlockers.any())
    {
        Square blocker = checkBlockers.poplsb();
        // this could technically pick the wrong pinner if there are 2 pinners
        // that are both aligned, but it's so rare that I doubt it matters
        Bitboard ray = attacks::alignedSquares(blocker, board.kingSq(them));
        Piece piece = board.pieceAt(blocker);
        Color color = getPieceColor(piece);
        PieceType pieceType = getPieceType(piece);
        // pinned
        if (color == them)
        {
            Square pinner = (board.pinners(them) & ray).lsb();
            PieceType pinnerPiece = getPieceType(board.pieceAt(pinner));

            eval += SAFETY_PINNED[static_cast<i32>(pieceType)]
                                 [static_cast<i32>(pinnerPiece) - static_cast<i32>(BISHOP)];
            TRACE_INC(safetyPinned[static_cast<i32>(pieceType)]
                                  [static_cast<i32>(pinnerPiece) - static_cast<i32>(BISHOP)]);
        }
        // discovered
        else
        {
            Square discoverer = (board.discoverers(them) & ray).lsb();
            PieceType discovererPiece = getPieceType(board.pieceAt(discoverer));

            eval += SAFETY_DISCOVERED[static_cast<i32>(pieceType)]
                                     [static_cast<i32>(discovererPiece) - static_cast<i32>(BISHOP)];
            TRACE_INC(safetyDiscovered[static_cast<i32>(pieceType)]
                                      [static_cast<i32>(discovererPiece) - static_cast<i32>(BISHOP)]);
        }
    }

    eval += SAFETY_OFFSET;
    TRACE_INC(safetyOffset);

    ScorePair safety = ScorePair(safetyAdjustment(eval.mg()), safetyAdjustment(eval.eg()));
    return safety;
}

ScorePair evaluateComplexity(
    const Board& board, const PawnStructure& pawnStructure, ScorePair eval, Trace& trace)
{
    constexpr Bitboard KING_SIDE = FILE_A_BB | FILE_B_BB | FILE_C_BB | FILE_D_BB;
    constexpr Bitboard QUEEN_SIDE = ~KING_SIDE;
    Bitboard pawns = board.pieces(PAWN);
    bool pawnsBothSides = (pawns & KING_SIDE).any() && (pawns & QUEEN_SIDE).any();
    bool pawnEndgame = board.allPieces() == (pawns | board.pieces(KING));

    trace.complexityPawns[WHITE] += pawns.popcount();
    trace.complexityPawnsBothSides[WHITE] += pawnsBothSides;
    trace.complexityPawnEndgame[WHITE] += pawnEndgame;
    trace.complexityOffset[WHITE] = 1;

    ScorePair complexity = COMPLEXITY_PAWNS * pawns.popcount()
        + COMPLEXITY_PAWNS_BOTH_SIDES * pawnsBothSides + COMPLEXITY_PAWN_ENDGAME * pawnEndgame
        + COMPLEXITY_OFFSET;

    i32 egSign = (eval.eg() > 0) - (eval.eg() < 0);

    i32 egComplexity = std::max(complexity.eg(), -std::abs(eval.eg()));

    return ScorePair(0, egSign * egComplexity);
}

template<Color us>
void initEvalData(const Board& board, EvalData& evalData, const PawnStructure& pawnStructure)
{
    constexpr Color them = ~us;
    Bitboard ourPawns = board.pieces(us, PAWN);
    Bitboard blockedPawns = ourPawns & attacks::pawnPushes<them>(board.allPieces());
    Square ourKing = board.kingSq(us);

    evalData.mobilityArea[us] =
        ~pawnStructure.pawnAttacks[them] & ~Bitboard::fromSquare(ourKing) & ~blockedPawns;
    evalData.attacked[us] = evalData.attackedBy[us][PAWN] = pawnStructure.pawnAttacks[us];

    Bitboard ourKingAtks = attacks::kingAttacks(ourKing);
    evalData.attackedBy[us][KING] = ourKingAtks;
    evalData.attackedBy2[us] = evalData.attacked[us] & ourKingAtks;
    evalData.attacked[us] |= ourKingAtks;
    evalData.kingRing[us] =
        (ourKingAtks | attacks::pawnPushes<us>(ourKingAtks)) & ~Bitboard::fromSquare(ourKing);
    if (FILE_H_BB.has(ourKing))
        evalData.kingRing[us] |= evalData.kingRing[us].west();
    if (FILE_A_BB.has(ourKing))
        evalData.kingRing[us] |= evalData.kingRing[us].east();

    evalData.kingFlank[us] = attacks::kingFlank(us, ourKing.file());
}

ScorePair evaluatePsqt(const Board& board, Trace& trace)
{
    ScorePair eval = ScorePair(0, 0);
    for (Color c : {WHITE, BLACK})
    {
        bool mirror = board.kingSq(c).file() >= FILE_E;
        for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING})
        {
            Bitboard pieces = board.pieces(c, pt);
            while (pieces.any())
            {
                Square sq = pieces.poplsb();
                // hack for now
                i32 x = 0;
                if (c == WHITE)
                    x ^= 56;
                if (mirror)
                    x ^= 7;
                trace.psqt[static_cast<i32>(pt)][sq.value() ^ x][c]++;
                ScorePair d =
                    MATERIAL[static_cast<i32>(pt)] + PSQT[static_cast<i32>(pt)][sq.value() ^ x];
                if (c == WHITE)
                    eval += d;
                else
                    eval -= d;
            }
        }
    }

    return eval;
}

double evaluateScale(const Board& board, ScorePair eval)
{
    Color strongSide = eval.eg() > 0 ? WHITE : eval.eg() < 0 ? BLACK : board.sideToMove();

    i32 strongPawns = board.pieces(strongSide, PAWN).popcount();

    return 80 + strongPawns * 7;
}

Trace getTrace(const Board& board)
{
    Trace trace = {};

    ScorePair eval = evaluatePsqt(board, trace);

    PawnStructure pawnStructure(board);

    EvalData evalData = {};
    initEvalData<WHITE>(board, evalData, pawnStructure);
    initEvalData<BLACK>(board, evalData, pawnStructure);

    eval += evaluatePawns(board, pawnStructure, trace);

    // clang-format off
    eval += evaluateKnightOutposts<WHITE>(board, pawnStructure, trace) - evaluateKnightOutposts<BLACK>(board, pawnStructure, trace);
    eval += evaluateBishopPawns<WHITE>(board, trace) - evaluateBishopPawns<BLACK>(board, trace);
    eval += evaluateRookOpen<WHITE>(board, trace) - evaluateRookOpen<BLACK>(board, trace);
    eval += evaluateMinorBehindPawn<WHITE>(board, trace) - evaluateMinorBehindPawn<BLACK>(board, trace);

    eval += evaluatePieces<WHITE, KNIGHT>(board, evalData, trace) - evaluatePieces<BLACK, KNIGHT>(board, evalData, trace);
    eval += evaluatePieces<WHITE, BISHOP>(board, evalData, trace) - evaluatePieces<BLACK, BISHOP>(board, evalData, trace);
    eval += evaluatePieces<WHITE, ROOK>(board, evalData, trace) - evaluatePieces<BLACK, ROOK>(board, evalData, trace);
    eval += evaluatePieces<WHITE, QUEEN>(board, evalData, trace) - evaluatePieces<BLACK, QUEEN>(board, evalData, trace);

    eval += evaluateKings<WHITE>(board, evalData, trace) - evaluateKings<BLACK>(board, evalData, trace);
    eval += evaluatePassedPawns<WHITE>(board, pawnStructure, evalData, trace) - evaluatePassedPawns<BLACK>(board, pawnStructure, evalData, trace);
    eval += evaluateThreats<WHITE>(board, evalData, trace) - evaluateThreats<BLACK>(board, evalData, trace);
    // clang-format on

    eval += evaluateComplexity(board, pawnStructure, eval, trace);

    trace.tempo[board.sideToMove()]++;

    trace.egScale = evaluateScale(board, eval) / 128.0;

    eval += (board.sideToMove() == WHITE ? TEMPO : -TEMPO);

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
    addCoefficientArray2D(trace.psqt, ParamType::NORMAL);

    addCoefficientArray2D(trace.mobility, ParamType::NORMAL);

    addCoefficientArray(trace.threatByPawn, ParamType::NORMAL);
    addCoefficientArray2D(trace.threatByKnight, ParamType::NORMAL);
    addCoefficientArray2D(trace.threatByBishop, ParamType::NORMAL);
    addCoefficientArray2D(trace.threatByRook, ParamType::NORMAL);
    addCoefficientArray2D(trace.threatByQueen, ParamType::NORMAL);
    addCoefficientArray(trace.threatByKing, ParamType::NORMAL);
    addCoefficient(trace.knightHitQueen, ParamType::NORMAL);
    addCoefficient(trace.bishopHitQueen, ParamType::NORMAL);
    addCoefficient(trace.rookHitQueen, ParamType::NORMAL);
    addCoefficient(trace.pushThreat, ParamType::NORMAL);
    addCoefficient(trace.restrictedSquares, ParamType::NORMAL);

    addCoefficientArray(trace.isolatedPawn, ParamType::NORMAL);
    addCoefficient(trace.isolatedExposed, ParamType::NORMAL);
    addCoefficientArray(trace.doubledPawn, ParamType::NORMAL);
    addCoefficientArray(trace.backwardsPawn, ParamType::NORMAL);
    addCoefficient(trace.backwardsExposed, ParamType::NORMAL);
    addCoefficientArray(trace.pawnPhalanx, ParamType::NORMAL);
    addCoefficientArray(trace.defendedPawn, ParamType::NORMAL);
    addCoefficientArray2D(trace.candidatePasser, ParamType::NORMAL);

    addCoefficientArray3D(trace.passedPawn, ParamType::NORMAL);
    addCoefficientArray(trace.ourPasserProximity, ParamType::NORMAL);
    addCoefficientArray(trace.theirPasserProximity, ParamType::NORMAL);
    addCoefficientArray(trace.passerDefendedPush, ParamType::NORMAL);
    addCoefficientArray(trace.passerSliderBehind, ParamType::NORMAL);

    addCoefficientArray3D(trace.pawnStorm, ParamType::SAFETY);
    addCoefficientArray2D(trace.pawnShield, ParamType::SAFETY);
    addCoefficient(trace.safeKnightCheck, ParamType::SAFETY);
    addCoefficient(trace.safeBishopCheck, ParamType::SAFETY);
    addCoefficient(trace.safeRookCheck, ParamType::SAFETY);
    addCoefficient(trace.safeQueenCheck, ParamType::SAFETY);
    addCoefficient(trace.unsafeKnightCheck, ParamType::SAFETY);
    addCoefficient(trace.unsafeBishopCheck, ParamType::SAFETY);
    addCoefficient(trace.unsafeRookCheck, ParamType::SAFETY);
    addCoefficient(trace.unsafeQueenCheck, ParamType::SAFETY);
    addCoefficient(trace.queenlessAttack, ParamType::SAFETY);
    addCoefficientArray(trace.kingAttackerWeight, ParamType::SAFETY);
    addCoefficient(trace.kingAttacks, ParamType::SAFETY);
    addCoefficient(trace.weakKingRing, ParamType::SAFETY);
    addCoefficientArray(trace.kingFlankAttacks, ParamType::SAFETY);
    addCoefficientArray(trace.kingFlankDefenses, ParamType::SAFETY);
    addCoefficientArray2D(trace.safetyPinned, ParamType::SAFETY);
    addCoefficientArray2D(trace.safetyDiscovered, ParamType::SAFETY);
    addCoefficient(trace.safetyOffset, ParamType::SAFETY);

    addCoefficient(trace.minorBehindPawn, ParamType::NORMAL);
    addCoefficient(trace.knightOutpost, ParamType::NORMAL);
    addCoefficientArray(trace.bishopPawns, ParamType::NORMAL);
    addCoefficient(trace.bishopPair, ParamType::NORMAL);
    addCoefficient(trace.longDiagBishop, ParamType::NORMAL);
    addCoefficientArray(trace.openRook, ParamType::NORMAL);

    addCoefficient(trace.tempo, ParamType::NORMAL);

    addCoefficient(trace.complexityPawns, ParamType::COMPLEXITY);
    addCoefficient(trace.complexityPawnsBothSides, ParamType::COMPLEXITY);
    addCoefficient(trace.complexityPawnEndgame, ParamType::COMPLEXITY);
    addCoefficient(trace.complexityOffset, ParamType::COMPLEXITY);

    return {pos, m_Coefficients.size(), trace.egScale};
}

template<typename T>
void addEvalParam(EvalParams& params, const T& t, ParamType type)
{
    params.linear.push_back({type, static_cast<double>(t.mg()), static_cast<double>(t.eg())});
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
    for (i32 i = 0; i < 6; i++)
        for (i32 j = (i == 0 ? 8 : 0); j < (i == 0 ? 56 : 64); j++)
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
    addEvalParam(params, KNIGHT_HIT_QUEEN, ParamType::NORMAL);
    addEvalParam(params, BISHOP_HIT_QUEEN, ParamType::NORMAL);
    addEvalParam(params, ROOK_HIT_QUEEN, ParamType::NORMAL);
    addEvalParam(params, PUSH_THREAT, ParamType::NORMAL);
    addEvalParam(params, RESTRICTED_SQUARES, ParamType::NORMAL);

    addEvalParamArray(params, ISOLATED_PAWN, ParamType::NORMAL);
    addEvalParam(params, ISOLATED_EXPOSED, ParamType::NORMAL);
    addEvalParamArray(params, DOUBLED_PAWN, ParamType::NORMAL);
    addEvalParamArray(params, BACKWARDS_PAWN, ParamType::NORMAL);
    addEvalParam(params, BACKWARDS_EXPOSED, ParamType::NORMAL);
    addEvalParamArray(params, PAWN_PHALANX, ParamType::NORMAL);
    addEvalParamArray(params, DEFENDED_PAWN, ParamType::NORMAL);
    addEvalParamArray2D(params, CANDIDATE_PASSER, ParamType::NORMAL);

    addEvalParamArray3D(params, PASSED_PAWN, ParamType::NORMAL);
    addEvalParamArray(params, OUR_PASSER_PROXIMITY, ParamType::NORMAL);
    addEvalParamArray(params, THEIR_PASSER_PROXIMITY, ParamType::NORMAL);
    addEvalParamArray(params, PASSER_DEFENDED_PUSH, ParamType::NORMAL);
    addEvalParamArray(params, PASSER_SLIDER_BEHIND, ParamType::NORMAL);

    addEvalParamArray3D(params, PAWN_STORM, ParamType::SAFETY);
    addEvalParamArray2D(params, PAWN_SHIELD, ParamType::SAFETY);
    addEvalParam(params, SAFE_KNIGHT_CHECK, ParamType::SAFETY);
    addEvalParam(params, SAFE_BISHOP_CHECK, ParamType::SAFETY);
    addEvalParam(params, SAFE_ROOK_CHECK, ParamType::SAFETY);
    addEvalParam(params, SAFE_QUEEN_CHECK, ParamType::SAFETY);
    addEvalParam(params, UNSAFE_KNIGHT_CHECK, ParamType::SAFETY);
    addEvalParam(params, UNSAFE_BISHOP_CHECK, ParamType::SAFETY);
    addEvalParam(params, UNSAFE_ROOK_CHECK, ParamType::SAFETY);
    addEvalParam(params, UNSAFE_QUEEN_CHECK, ParamType::SAFETY);
    addEvalParam(params, QUEENLESS_ATTACK, ParamType::SAFETY);
    addEvalParamArray(params, KING_ATTACKER_WEIGHT, ParamType::SAFETY);
    addEvalParam(params, KING_ATTACKS, ParamType::SAFETY);
    addEvalParam(params, WEAK_KING_RING, ParamType::SAFETY);
    addEvalParamArray(params, KING_FLANK_ATTACKS, ParamType::SAFETY);
    addEvalParamArray(params, KING_FLANK_DEFENSES, ParamType::SAFETY);
    addEvalParamArray2D(params, SAFETY_PINNED, ParamType::SAFETY);
    addEvalParamArray2D(params, SAFETY_DISCOVERED, ParamType::SAFETY);
    addEvalParam(params, SAFETY_OFFSET, ParamType::SAFETY);

    addEvalParam(params, MINOR_BEHIND_PAWN, ParamType::NORMAL);
    addEvalParam(params, KNIGHT_OUTPOST, ParamType::NORMAL);
    addEvalParamArray(params, BISHOP_PAWNS, ParamType::NORMAL);
    addEvalParam(params, BISHOP_PAIR, ParamType::NORMAL);
    addEvalParam(params, LONG_DIAG_BISHOP, ParamType::NORMAL);
    addEvalParamArray(params, ROOK_OPEN, ParamType::NORMAL);
    addEvalParam(params, TEMPO, ParamType::NORMAL);

    addEvalParam(params, COMPLEXITY_PAWNS, ParamType::COMPLEXITY);
    addEvalParam(params, COMPLEXITY_PAWNS_BOTH_SIDES, ParamType::COMPLEXITY);
    addEvalParam(params, COMPLEXITY_PAWN_ENDGAME, ParamType::COMPLEXITY);
    addEvalParam(params, COMPLEXITY_OFFSET, ParamType::COMPLEXITY);

    return params;
}

EvalParams EvalFn::getMaterialParams()
{
    EvalParams params = getInitialParams();
    for (auto& param : params.linear)
        param.mg = param.eg = 0;

    for (i32 i = 0; i < 6; i++)
        for (i32 j = (i == 0 ? 8 : 0); j < (i == 0 ? 56 : 64); j++)
        {
            params[i * 64 + j].mg += MATERIAL[i].mg();
            params[i * 64 + j].eg += MATERIAL[i].eg();
        }
    return params;
}

EvalParams EvalFn::getKParams()
{
    constexpr ScorePair K_MATERIAL[6] = {
        {67, 98}, {311, 409}, {330, 419}, {426, 746}, {860, 1403}, {0, 0}};

    EvalParams params = getInitialParams();
    for (auto& param : params.linear)
        param.mg = param.eg = 0;

    for (i32 i = 0; i < 6; i++)
        for (i32 j = (i == 0 ? 8 : 0); j < (i == 0 ? 56 : 64); j++)
        {
            params[i * 64 + j].mg += K_MATERIAL[i].mg();
            params[i * 64 + j].eg += K_MATERIAL[i].eg();
        }
    return params;
}

struct PrintState
{
    const EvalParams& params;
    u32 index;
    std::ostringstream ss;
};

template<i32 ALIGN_SIZE>
void printSingle(PrintState& state)
{
    const EvalParam& param = state.params[state.index++];
    if constexpr (ALIGN_SIZE == 0)
        state.ss << "S(" << param.mg << ", " << param.eg << ')';
    else
        state.ss << "S(" << std::setw(ALIGN_SIZE) << static_cast<i32>(param.mg) << ", "
                 << std::setw(ALIGN_SIZE) << static_cast<i32>(param.eg) << ')';
}

template<i32 ALIGN_SIZE>
void printPSQTs(PrintState& state)
{
    state.ss << "constexpr ScorePair PSQT[6][64] = {\n";
    for (i32 pce = 0; pce < 6; pce++)
    {
        state.ss << "    {\n";
        for (i32 y = 0; y < 8; y++)
        {
            state.ss << "        ";
            for (i32 x = 0; x < 8; x++)
            {
                printSingle<ALIGN_SIZE>(state);
                state.ss << ",";
                if (x != 7)
                    state.ss << " ";
            }
            state.ss << "\n";
        }
        state.ss << "    },\n";
    }
    state.ss << "};\n";
}

void printMaterial(PrintState& state)
{
    state.ss << "constexpr ScorePair MATERIAL[6] = {";
    for (i32 j = 0; j < 5; j++)
    {
        printSingle<4>(state);
        state.ss << ", ";
    }
    state.ss << "S(0, 0)};\n";
}

template<i32 ALIGN_SIZE>
void printArray(PrintState& state, i32 len)
{
    state.ss << '{';
    for (i32 i = 0; i < len; i++)
    {
        printSingle<ALIGN_SIZE>(state);
        if (i != len - 1)
            state.ss << ", ";
    }
    state.ss << '}';
}

template<i32 ALIGN_SIZE>
void printArray2D(PrintState& state, i32 outerLen, i32 innerLen, bool indent = false)
{
    state.ss << "{\n";
    for (i32 i = 0; i < outerLen; i++)
    {
        state.ss << "    ";
        if (indent)
            state.ss << "    ";
        printArray<ALIGN_SIZE>(state, innerLen);
        if (i != outerLen - 1)
            state.ss << ',';
        state.ss << '\n';
    }
    if (indent)
        state.ss << "    ";
    state.ss << "}";
}

template<i32 ALIGN_SIZE>
void printArray3D(PrintState& state, i32 len1, i32 len2, i32 len3)
{
    state.ss << "{\n";
    for (i32 i = 0; i < len1; i++)
    {
        state.ss << "    ";
        printArray2D<ALIGN_SIZE>(state, len2, len3, true);
        if (i != len1 - 1)
            state.ss << ',';
        state.ss << "\n";
    }
    state.ss << "}";
}

template<i32 ALIGN_SIZE>
void printRestParams(PrintState& state)
{
    state.ss << "constexpr ScorePair MOBILITY[4][28] = ";
    printArray2D<ALIGN_SIZE>(state, 4, 28);
    state.ss << ";\n";

    state.ss << '\n';

    state.ss << "constexpr ScorePair THREAT_BY_PAWN[6] = ";
    printArray<ALIGN_SIZE>(state, 6);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair THREAT_BY_KNIGHT[2][6] = ";
    printArray2D<ALIGN_SIZE>(state, 2, 6);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair THREAT_BY_BISHOP[2][6] = ";
    printArray2D<ALIGN_SIZE>(state, 2, 6);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair THREAT_BY_ROOK[2][6] = ";
    printArray2D<ALIGN_SIZE>(state, 2, 6);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair THREAT_BY_QUEEN[2][6] = ";
    printArray2D<ALIGN_SIZE>(state, 2, 6);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair THREAT_BY_KING[6] = ";
    printArray<ALIGN_SIZE>(state, 6);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair KNIGHT_HIT_QUEEN = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair BISHOP_HIT_QUEEN = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair ROOK_HIT_QUEEN = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair PUSH_THREAT = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair RESTRICTED_SQUARES = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << '\n';

    state.ss << "constexpr ScorePair ISOLATED_PAWN[4] = ";
    printArray<ALIGN_SIZE>(state, 4);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair ISOLATED_EXPOSED = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair DOUBLED_PAWN[4] = ";
    printArray<ALIGN_SIZE>(state, 4);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair BACKWARDS_PAWN[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair BACKWARDS_EXPOSED = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair PAWN_PHALANX[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair DEFENDED_PAWN[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair CANDIDATE_PASSER[2][8] = ";
    printArray2D<ALIGN_SIZE>(state, 2, 8);
    state.ss << ";\n";

    state.ss << '\n';

    state.ss << "constexpr ScorePair PASSED_PAWN[2][2][8] = ";
    printArray3D<ALIGN_SIZE>(state, 2, 2, 8);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair OUR_PASSER_PROXIMITY[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair THEIR_PASSER_PROXIMITY[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair PASSER_DEFENDED_PUSH[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair PASSER_SLIDER_BEHIND[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << '\n';

    state.ss << "constexpr ScorePair PAWN_STORM[2][4][8] = ";
    printArray3D<ALIGN_SIZE>(state, 2, 4, 8);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair PAWN_SHIELD[4][8] = ";
    printArray2D<ALIGN_SIZE>(state, 4, 8);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair SAFE_KNIGHT_CHECK = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair SAFE_BISHOP_CHECK = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair SAFE_ROOK_CHECK = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair SAFE_QUEEN_CHECK = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair UNSAFE_KNIGHT_CHECK = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair UNSAFE_BISHOP_CHECK = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair UNSAFE_ROOK_CHECK = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair UNSAFE_QUEEN_CHECK = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair QUEENLESS_ATTACK = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair KING_ATTACKER_WEIGHT[4] = ";
    printArray<ALIGN_SIZE>(state, 4);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair KING_ATTACKS = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair WEAK_KING_RING = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair KING_FLANK_ATTACKS[2] = ";
    printArray<ALIGN_SIZE>(state, 2);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair KING_FLANK_DEFENSES[2] = ";
    printArray<ALIGN_SIZE>(state, 2);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair SAFETY_PINNED[5][3] = ";
    printArray2D<ALIGN_SIZE>(state, 5, 3);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair SAFETY_DISCOVERED[6][3] = ";
    printArray2D<ALIGN_SIZE>(state, 6, 3);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair SAFETY_OFFSET = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << '\n';

    state.ss << "constexpr ScorePair MINOR_BEHIND_PAWN = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair KNIGHT_OUTPOST = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair BISHOP_PAWNS[7] = ";
    printArray<ALIGN_SIZE>(state, 7);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair BISHOP_PAIR = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair LONG_DIAG_BISHOP = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair ROOK_OPEN[2] = ";
    printArray<ALIGN_SIZE>(state, 2);
    state.ss << ";\n";

    state.ss << '\n';

    state.ss << "constexpr ScorePair TEMPO = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << '\n';

    state.ss << "constexpr ScorePair COMPLEXITY_PAWNS = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair COMPLEXITY_PAWNS_BOTH_SIDES = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair COMPLEXITY_PAWN_ENDGAME = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr ScorePair COMPLEXITY_OFFSET = ";
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

std::array<i32, 2> avgValue(EvalParams& params, i32 offset, i32 size)
{
    std::array<i32, 2> avg = {};
    for (i32 i = offset; i < offset + size; i++)
    {
        avg[0] += static_cast<i32>(params[i].mg);
        avg[1] += static_cast<i32>(params[i].eg);
    }
    avg[0] /= size;
    avg[1] /= size;
    return avg;
}

void rebalance(std::array<i32, 2> factor, std::array<i32, 2>& materialValue, EvalParams& params,
    i32 offset, i32 size)
{
    materialValue[0] += factor[0];
    materialValue[1] += factor[1];

    for (i32 i = offset; i < offset + size; i++)
    {
        params[i].mg -= static_cast<double>(factor[0]);
        params[i].eg -= static_cast<double>(factor[1]);
    }
}

EvalParams extractMaterial(const EvalParams& params)
{
    EvalParams rebalanced = params;
    for (auto& elem : rebalanced.linear)
    {
        elem.mg = std::round(elem.mg);
        elem.eg = std::round(elem.eg);
    }

    std::array<std::array<i32, 2>, 6> material = {};

    // psqts
    for (i32 pce = 0; pce < 6; pce++)
    {
        if (pce == 0)
        {
            auto avg = avgValue(rebalanced, TRACE_OFFSET(psqt[0]) + 24, TRACE_SIZE(psqt[0]) - 32);
            rebalance(avg, material[pce], rebalanced, TRACE_OFFSET(psqt[0]) + 8,
                TRACE_SIZE(psqt[0]) - 16);
        }
        else
        {
            auto avg = avgValue(rebalanced, TRACE_OFFSET(psqt[pce]), TRACE_SIZE(psqt[pce]));
            rebalance(avg, material[pce], rebalanced, TRACE_OFFSET(psqt[pce]), TRACE_SIZE(psqt[pce]));
        }

        if (pce == 5)
        {
            i32 offset = TRACE_OFFSET(psqt[pce]);
            for (i32 rank = 0; rank < 8; rank++)
            {
                for (i32 file = 4; file < 8; file++)
                {
                    rebalanced[offset + rank * 8 + file].mg = 0;
                    rebalanced[offset + rank * 8 + file].eg = 0;
                }
            }
        }
    }

    // mobility
    rebalance(avgValue(rebalanced, TRACE_OFFSET(mobility[0]), 9), material[1], rebalanced,
        TRACE_OFFSET(mobility[0]), 9);
    rebalance(avgValue(rebalanced, TRACE_OFFSET(mobility[1]), 14), material[2], rebalanced,
        TRACE_OFFSET(mobility[1]), 14);
    rebalance(avgValue(rebalanced, TRACE_OFFSET(mobility[2]), 15), material[3], rebalanced,
        TRACE_OFFSET(mobility[2]), 15);
    rebalance(avgValue(rebalanced, TRACE_OFFSET(mobility[3]), 28), material[4], rebalanced,
        TRACE_OFFSET(mobility[3]), 28);

    // bishop pawns
    rebalance(avgValue(rebalanced, TRACE_OFFSET(bishopPawns), TRACE_SIZE(bishopPawns)), material[2],
        rebalanced, TRACE_OFFSET(bishopPawns), TRACE_SIZE(bishopPawns));

    EvalParams extracted;
    for (i32 i = 0; i < 5; i++)
    {
        extracted.linear.push_back(EvalParam{ParamType::NORMAL, static_cast<double>(material[i][0]),
            static_cast<double>(material[i][1])});
    }
    extracted.linear.insert(extracted.linear.end(), rebalanced.linear.begin(), rebalanced.linear.end());
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
