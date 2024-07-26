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
    ColorArray<PackedScore> attackWeight;
    ColorArray<int> attackCount;

    Bitboard passedPawns;
};

#define TRACE_INC(traceElem) trace.traceElem[us]++
#define TRACE_ADD(traceElem, amount) trace.traceElem[us] += amount

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

        eval += MOBILITY[static_cast<int>(piece) - static_cast<int>(PieceType::KNIGHT)][(attacks & evalData.mobilityArea[us]).popcount()];
        TRACE_INC(mobility[static_cast<int>(piece) - static_cast<int>(PieceType::KNIGHT)][(attacks & evalData.mobilityArea[us]).popcount()]);

        if (Bitboard kingRingAtks = evalData.kingRing[them] & attacks; kingRingAtks.any())
        {
            evalData.attackWeight[us] += KING_ATTACKER_WEIGHT[static_cast<int>(piece) - static_cast<int>(PieceType::KNIGHT)];
            TRACE_INC(kingAttackerWeight[static_cast<int>(piece) - static_cast<int>(PieceType::KNIGHT)]);
            evalData.attackCount[us] += kingRingAtks.popcount();
        }

        Bitboard fileBB = Bitboard::fileBB(fileOf(sq));

        if constexpr (piece == PieceType::ROOK)
        {
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

        if constexpr (piece == PieceType::KNIGHT)
        {
            Bitboard outposts = outpostSquares & ~evalData.pawnAttackSpans[them] & evalData.attackedBy[us][PieceType::PAWN];
            if ((Bitboard::fromSquare(sq) & outposts).any())
            {
                eval += KNIGHT_OUTPOST;
                TRACE_INC(knightOutpost);
            }
        }

        if constexpr (piece == PieceType::BISHOP)
        {
            bool lightSquare = (Bitboard::fromSquare(sq) & LIGHT_SQUARES).any();
            Bitboard sameColorPawns = board.pieces(us, PieceType::PAWN) & (lightSquare ? LIGHT_SQUARES : DARK_SQUARES);
            eval += BISHOP_PAWNS[std::min(sameColorPawns.popcount(), 6u)];
            TRACE_INC(bishopPawns[std::min(sameColorPawns.popcount(), 6u)]);
        }
    }

    return eval;
}



template<Color us>
PackedScore evaluatePawns(const Board& board, EvalData& evalData, Trace& trace)
{
    Bitboard ourPawns = board.pieces(us, PieceType::PAWN);

    PackedScore eval{0, 0};

    Bitboard pawns = ourPawns;
    while (pawns.any())
    {
        uint32_t sq = pawns.poplsb();
        if (board.isPassedPawn(sq))
        {
            evalData.passedPawns |= Bitboard::fromSquare(sq);
            eval += PASSED_PAWN[relativeRankOf<us>(sq)];
            TRACE_INC(passedPawn[relativeRankOf<us>(sq)]);
        }
        if (board.isIsolatedPawn(sq))
        {
            eval += ISOLATED_PAWN[fileOf(sq)];
            TRACE_INC(isolatedPawn[fileOf(sq)]);
        }
    }

    Bitboard phalanx = ourPawns & ourPawns.west();
    while (phalanx.any())
    {
        uint32_t sq = phalanx.poplsb();
        eval += PAWN_PHALANX[relativeRankOf<us>(sq)];
        TRACE_INC(pawnPhalanx[relativeRankOf<us>(sq)]);
    }

    Bitboard defended = ourPawns & attacks::pawnAttacks<us>(ourPawns);
    while (defended.any())
    {
        uint32_t sq = defended.poplsb();
        eval += DEFENDED_PAWN[relativeRankOf<us>(sq)];
        TRACE_INC(defendedPawn[relativeRankOf<us>(sq)]);
    }

    return eval;
}

template<Color us>
PackedScore evaluateKingPawn(const Board & board, const EvalData & evalData, Trace& trace)
{
    constexpr Color them = ~us;
    uint32_t ourKing = board.pieces(us, PieceType::KING).lsb();
    uint32_t theirKing = board.pieces(them, PieceType::KING).lsb();

    Bitboard passers = evalData.passedPawns & board.pieces(us);

    PackedScore eval{0, 0};

    while (passers.any())
    {
        uint32_t passer = passers.poplsb();
        eval += OUR_PASSER_PROXIMITY[chebyshev(ourKing, passer)];
        eval += THEIR_PASSER_PROXIMITY[chebyshev(theirKing, passer)];
        TRACE_INC(ourPasserProximity[chebyshev(ourKing, passer)]);
        TRACE_INC(theirPasserProximity[chebyshev(theirKing, passer)]);
    }

    return eval;
}

PackedScore evaluatePawns(const Board& board, EvalData& evalData, Trace& trace)
{
    return evaluatePawns<Color::WHITE>(board, evalData, trace) - evaluatePawns<Color::BLACK>(board, evalData, trace);
}

template<Color us>
PackedScore evaluateThreats(const Board& board, const EvalData& evalData, Trace& trace)
{
    constexpr Color them = ~us;

    PackedScore eval{0, 0};

    Bitboard defendedBB = evalData.attacked[them];

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
        int threat = knightThreats.poplsb();
        PieceType threatened = getPieceType(board.pieceAt(threat));
        bool defended = (defendedBB & Bitboard::fromSquare(threat)).any();
        eval += THREAT_BY_KNIGHT[defended][static_cast<int>(threatened)];
        TRACE_INC(threatByKnight[defended][static_cast<int>(threatened)]);
    }

    Bitboard bishopThreats = evalData.attackedBy[us][PieceType::BISHOP] & board.pieces(them);
    while (bishopThreats.any())
    {
        int threat = bishopThreats.poplsb();
        PieceType threatened = getPieceType(board.pieceAt(threat));
        bool defended = (defendedBB & Bitboard::fromSquare(threat)).any();
        eval += THREAT_BY_BISHOP[defended][static_cast<int>(threatened)];
        TRACE_INC(threatByBishop[defended][static_cast<int>(threatened)]);
    }

    Bitboard rookThreats = evalData.attackedBy[us][PieceType::ROOK] & board.pieces(them);
    while (rookThreats.any())
    {
        int threat = rookThreats.poplsb();
        PieceType threatened = getPieceType(board.pieceAt(threat));
        bool defended = (defendedBB & Bitboard::fromSquare(threat)).any();
        eval += THREAT_BY_ROOK[defended][static_cast<int>(threatened)];
        TRACE_INC(threatByRook[defended][static_cast<int>(threatened)]);
    }

    Bitboard queenThreats = evalData.attackedBy[us][PieceType::QUEEN] & board.pieces(them);
    while (queenThreats.any())
    {
        int threat = queenThreats.poplsb();
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

    Bitboard safe = ~defendedBB | (evalData.attacked[us] & ~evalData.attackedBy[them][PieceType::PAWN]);
    Bitboard pushes = attacks::pawnPushes<us>(board.pieces(us, PieceType::PAWN)) & ~board.allPieces();
    pushes |= attacks::pawnPushes<us>(pushes & Bitboard::nthRank<us, 2>()) & ~board.allPieces();

    Bitboard pushThreats = attacks::pawnAttacks<us>(pushes & safe) & nonPawnEnemies;
    eval += PUSH_THREAT * pushThreats.popcount();
    TRACE_ADD(pushThreat, pushThreats.popcount());

    return eval;
}

template<Color us>
PackedScore evalKingPawnFile(uint32_t file, Bitboard ourPawns, Bitboard theirPawns, uint32_t theirKing, Trace& trace)
{
    PackedScore eval{0, 0};
    uint32_t kingFile = fileOf(theirKing);
    {
        Bitboard filePawns = ourPawns & Bitboard::fileBB(file);
        // 4 = e file
        int idx = (kingFile == file) ? 1 : (kingFile >= 4) == (kingFile < file) ? 0 : 2;

        int rankDist = filePawns.any() ?
            std::abs(rankOf(us == Color::WHITE ? filePawns.msb() : filePawns.lsb()) - rankOf(theirKing)) :
            7;
        eval += PAWN_STORM[idx][rankDist];
        TRACE_INC(pawnStorm[idx][rankDist]);
    }
    {
        Bitboard filePawns = theirPawns & Bitboard::fileBB(file);
        // 4 = e file
        int idx = (kingFile == file) ? 1 : (kingFile >= 4) == (kingFile < file) ? 0 : 2;
        int rankDist = filePawns.any() ?
            std::abs(rankOf(us == Color::WHITE ? filePawns.msb() : filePawns.lsb()) - rankOf(theirKing)) :
            7;
        eval += PAWN_SHIELD[idx][rankDist];
        TRACE_INC(pawnShield[idx][rankDist]);
    }

    return eval;
}

template<Color us>
PackedScore evaluateKings(const Board& board, const EvalData& evalData, Trace& trace)
{
    constexpr Color them = ~us;
    Bitboard ourPawns = board.pieces(us, PieceType::PAWN);
    Bitboard theirPawns = board.pieces(them, PieceType::PAWN);

    uint32_t theirKing = board.pieces(them, PieceType::KING).lsb();

    PackedScore eval{0, 0};

    for (uint32_t file = 0; file < 8; file++)
        eval += evalKingPawnFile<us>(file, ourPawns, theirPawns, theirKing, trace);

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

    eval += SAFE_KNIGHT_CHECK * (knightChecks & safe).popcount();
    eval += SAFE_BISHOP_CHECK * (bishopChecks & safe).popcount();
    eval += SAFE_ROOK_CHECK * (rookChecks & safe).popcount();
    eval += SAFE_QUEEN_CHECK * (queenChecks & safe).popcount();

    eval += evalData.attackWeight[us];
    eval += KING_ATTACKS[attackCount];

    return eval;
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

PackedScore evaluatePsqt(const Board& board, Trace& trace)
{
    PackedScore eval{0, 0};
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
                eval += MATERIAL[static_cast<int>(pt)] + PSQT[static_cast<int>(pt)][sq];
            }
        }

    return eval;
}

Trace getTrace(const Board& board)
{
    EvalData evalData = {};
    initEvalData(board, evalData);

    Trace trace = {};

    PackedScore eval = evaluatePsqt(board, trace);

    eval += evaluatePieces<Color::WHITE, PieceType::KNIGHT>(board, evalData, trace) - evaluatePieces<Color::BLACK, PieceType::KNIGHT>(board, evalData, trace);
    eval += evaluatePieces<Color::WHITE, PieceType::BISHOP>(board, evalData, trace) - evaluatePieces<Color::BLACK, PieceType::BISHOP>(board, evalData, trace);
    eval += evaluatePieces<Color::WHITE, PieceType::ROOK>(board, evalData, trace) - evaluatePieces<Color::BLACK, PieceType::ROOK>(board, evalData, trace);
    eval += evaluatePieces<Color::WHITE, PieceType::QUEEN>(board, evalData, trace) - evaluatePieces<Color::BLACK, PieceType::QUEEN>(board, evalData, trace);

    eval += evaluateKings<Color::WHITE>(board, evalData, trace) - evaluateKings<Color::BLACK>(board, evalData, trace);

    eval += evaluatePawns(board, evalData, trace);
    eval += evaluateKingPawn<Color::WHITE>(board, evalData, trace) - evaluateKingPawn<Color::BLACK>(board, evalData, trace);
    eval += evaluateThreats<Color::WHITE>(board, evalData, trace) - evaluateThreats<Color::BLACK>(board, evalData, trace);

    trace.tempo[board.sideToMove()]++;

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

template<typename T>
void addEvalParam(EvalParams& params, const T& t)
{
    params.push_back({static_cast<double>(t.mg()), static_cast<double>(t.eg())});
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
            params[i * 64 + j].mg += MATERIAL[i].mg();
            params[i * 64 + j].eg += MATERIAL[i].eg();
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
            params[i * 64 + j].mg += MATERIAL[i].mg();
            params[i * 64 + j].eg += MATERIAL[i].eg();
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

    state.ss << "constexpr PackedScore PASSED_PAWN[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore ISOLATED_PAWN[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore PAWN_PHALANX[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore DEFENDED_PAWN[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore OUR_PASSER_PROXIMITY[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore THEIR_PASSER_PROXIMITY[8] = ";
    printArray<ALIGN_SIZE>(state, 8);
    state.ss << ";\n";

    state.ss << '\n';

    state.ss << "constexpr PackedScore PAWN_STORM[3][8] = ";
    printArray2D<ALIGN_SIZE>(state, 3, 8);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore PAWN_SHIELD[3][8] = ";
    printArray2D<ALIGN_SIZE>(state, 3, 8);
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

    state.ss << "constexpr PackedScore KING_ATTACKER_WEIGHT[4] = ";
    printArray<ALIGN_SIZE>(state, 4);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore KING_ATTACKS[14] = ";
    printArray<ALIGN_SIZE>(state, 14);
    state.ss << ";\n";

    state.ss << '\n';

    state.ss << "constexpr PackedScore KNIGHT_OUTPOST = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam BISHOP_PAWNS[7] = ";
    printArray<ALIGN_SIZE>(state, 7);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam BISHOP_PAIR = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr PackedScore ROOK_OPEN[2] = ";
    printArray<ALIGN_SIZE>(state, 2);
    state.ss << ";\n";

    state.ss << '\n';

    state.ss << "constexpr PackedScore TEMPO = ";
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
