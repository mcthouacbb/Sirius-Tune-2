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

struct LinearTrace
{
    TraceElem psqt[6][64];

    TraceElem mobility[4][28];

    TraceElem threats[6][6];

    TraceElem passedPawn[8];
    TraceElem isolatedPawn[8];
    TraceElem pawnPhalanx[8];
    TraceElem defendedPawn[8];

    TraceElem knightOutpost;
    TraceElem bishopPair;
    TraceElem openRook[2];
};

struct SafetyTrace
{
    TraceElem pawnStorm[3][8];
    TraceElem pawnShield[3][8];
    TraceElem safeKnightCheck;
    TraceElem safeBishopCheck;
    TraceElem safeRookCheck;
    TraceElem safeQueenCheck;
};

struct Trace
{
    LinearTrace linear;
    SafetyTrace safety;
};

struct EvalData
{
    ColorArray<Bitboard> mobilityArea;
    ColorArray<Bitboard> attacked;
    ColorArray<PieceTypeArray<Bitboard>> attackedBy;
    ColorArray<Bitboard> pawnAttackSpans;
};

#define TRACE_INC(traceElem) trace.traceElem[us]++
#define TRACE_ADD(traceElem, amount) trace.traceElem[us] += amount

template<Color us, PieceType piece>
void evaluatePieces(const Board& board, EvalData& evalData, LinearTrace& trace)
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

        if constexpr (piece == PieceType::KNIGHT)
        {
            Bitboard outposts = outpostSquares & ~evalData.pawnAttackSpans[them] & evalData.attackedBy[us][PieceType::PAWN];
            if (Bitboard::fromSquare(sq) & outposts)
                TRACE_INC(knightOutpost);
        }
    }
}



template<Color us>
void evaluatePawns(const Board& board, LinearTrace& trace)
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

void evaluatePawns(const Board& board, LinearTrace& trace)
{
    evaluatePawns<Color::WHITE>(board, trace);
    evaluatePawns<Color::BLACK>(board, trace);
}

// I'll figure out how to add the other pieces here later
template<Color us>
void evaluateThreats(const Board& board, const EvalData& evalData, LinearTrace& trace)
{
    constexpr Color them = ~us;
    Bitboard threats = evalData.attackedBy[us][PieceType::PAWN] & board.getColor(them);
    while (threats)
        TRACE_INC(threats[static_cast<int>(PieceType::PAWN)][static_cast<int>(getPieceType(board.getPieceAt(threats.poplsb())))]);
}

template<Color us>
void evalKingPawnFile(uint32_t file, Bitboard ourPawns, Bitboard theirPawns, uint32_t theirKing, SafetyTrace& trace)
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
void evaluateKings(const Board& board, const EvalData& evalData, SafetyTrace& trace)
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
    evalData.pawnAttackSpans[Color::WHITE] = attacks::fillUp<Color::WHITE>(whitePawnAttacks);
    evalData.attacked[Color::WHITE] = evalData.attackedBy[Color::WHITE][PieceType::PAWN] = whitePawnAttacks;

    evalData.mobilityArea[Color::BLACK] = ~whitePawnAttacks;
    evalData.pawnAttackSpans[Color::BLACK] = attacks::fillUp<Color::BLACK>(blackPawnAttacks);
    evalData.attacked[Color::BLACK] = evalData.attackedBy[Color::BLACK][PieceType::PAWN] = blackPawnAttacks;
}

void evaluatePsqt(const Board& board, LinearTrace& trace)
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

    evaluatePsqt(board, trace.linear);

    evaluatePieces<Color::WHITE, PieceType::KNIGHT>(board, evalData, trace.linear);
    evaluatePieces<Color::BLACK, PieceType::KNIGHT>(board, evalData, trace.linear);
    evaluatePieces<Color::WHITE, PieceType::BISHOP>(board, evalData, trace.linear);
    evaluatePieces<Color::BLACK, PieceType::BISHOP>(board, evalData, trace.linear);
    evaluatePieces<Color::WHITE, PieceType::ROOK>(board, evalData, trace.linear);
    evaluatePieces<Color::BLACK, PieceType::ROOK>(board, evalData, trace.linear);
    evaluatePieces<Color::WHITE, PieceType::QUEEN>(board, evalData, trace.linear);
    evaluatePieces<Color::BLACK, PieceType::QUEEN>(board, evalData, trace.linear);

    evaluateKings<Color::WHITE>(board, evalData, trace.safety);
    evaluateKings<Color::BLACK>(board, evalData, trace.safety);

    evaluatePawns(board, trace.linear);
    evaluateThreats<Color::WHITE>(board, evalData, trace.linear);
    evaluateThreats<Color::BLACK>(board, evalData, trace.linear);

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
    const auto& linear = trace.linear;
    const auto& safety = trace.safety;
    addCoefficientArray2D(linear.psqt, CoeffType::LINEAR);

    addCoefficientArray2D(linear.mobility, CoeffType::LINEAR);

    addCoefficientArray2D(linear.threats, CoeffType::LINEAR);

    addCoefficientArray(linear.passedPawn, CoeffType::LINEAR);
    addCoefficientArray(linear.isolatedPawn, CoeffType::LINEAR);
    addCoefficientArray(linear.pawnPhalanx, CoeffType::LINEAR);
    addCoefficientArray(linear.defendedPawn, CoeffType::LINEAR);

    addCoefficient(linear.knightOutpost, CoeffType::LINEAR);
    addCoefficient(linear.bishopPair, CoeffType::LINEAR);
    addCoefficientArray(linear.openRook, CoeffType::LINEAR);

    addCoefficientArray2D(safety.pawnStorm, CoeffType::SAFETY);
    addCoefficientArray2D(safety.pawnShield, CoeffType::SAFETY);
    addCoefficient(safety.safeKnightCheck, CoeffType::SAFETY);
    addCoefficient(safety.safeBishopCheck, CoeffType::SAFETY);
    addCoefficient(safety.safeRookCheck, CoeffType::SAFETY);
    addCoefficient(safety.safeQueenCheck, CoeffType::SAFETY);
    // safety offset
    addCoefficient(TraceElem{1, 1}, CoeffType::SAFETY);
    return {pos, m_Coefficients.size()};
}

using InitialParam = std::array<int, 2>;

#define S(mg, eg) {mg, eg}

constexpr InitialParam MATERIAL[6] = {S(  49,   78), S( 277,  341), S( 282,  356), S( 387,  626), S( 817, 1187), S(0, 0)};

constexpr InitialParam PSQT[6][64] = {
	{
		S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
		S(  36,  103), S(  48,   99), S(  31,  100), S(  67,   51), S(  45,   53), S(  32,   60), S( -30,  109), S( -34,  106),
		S(  13,   49), S(   7,   64), S(  32,   18), S(  39,  -17), S(  46,  -20), S(  77,   -7), S(  38,   44), S(  22,   34),
		S(  -6,   25), S(  -1,   27), S(   4,    4), S(   4,  -10), S(  24,   -9), S(  26,  -10), S(  10,   19), S(   9,    2),
		S( -15,    4), S( -10,   18), S(  -4,   -4), S(   5,   -8), S(   7,   -6), S(  11,   -9), S(   2,   10), S(  -4,  -12),
		S( -23,   -3), S( -17,    7), S( -12,   -6), S(  -9,   -3), S(   0,   -1), S(  -5,   -5), S(   5,    1), S(  -9,  -18),
		S( -13,    2), S(  -5,   14), S(  -1,    1), S(  -2,   -2), S(   5,    9), S(  29,   -5), S(  22,    4), S(  -2,  -16),
		S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
	},
	{
		S(-108,  -26), S(-100,   -4), S( -64,    8), S( -15,   -7), S(  18,   -4), S( -35,  -23), S( -76,   -2), S( -74,  -46),
		S( -18,    3), S(  -6,    9), S(   5,    3), S(  28,    1), S(  28,   -8), S(  55,  -20), S(  16,   -2), S(  23,  -16),
		S(  -2,   -3), S(   5,    1), S(  16,    5), S(  31,    3), S(  50,   -6), S(  80,  -27), S(  29,  -13), S(  44,  -18),
		S(   1,   11), S(   7,    4), S(  23,    7), S(  43,    7), S(  45,    4), S(  50,   -1), S(  32,    0), S(  42,   -5),
		S(  -6,   11), S(   4,    1), S(  12,   13), S(  21,   13), S(  22,   19), S(  25,    3), S(  26,   -2), S(   7,    9),
		S( -19,    4), S(  -8,    3), S(  -2,    5), S(   1,   17), S(  13,   15), S(   2,    0), S(  13,   -1), S(  -2,    6),
		S( -23,    8), S( -19,   11), S( -11,    2), S(   3,    5), S(   2,    3), S(   1,   -1), S(   2,    0), S(  -2,   21),
		S( -54,   18), S( -14,    1), S( -29,    1), S( -17,    3), S( -13,    7), S(  -6,   -4), S( -11,    7), S( -28,   17),
	},
	{
		S( -20,    5), S( -47,   10), S( -39,    3), S( -77,   12), S( -60,    8), S( -56,    1), S( -24,    1), S( -48,   -2),
		S( -16,   -7), S( -17,   -1), S( -14,   -2), S( -17,   -1), S(  -4,   -9), S(  -3,   -9), S( -26,    3), S( -12,   -6),
		S(   2,    8), S(  11,    0), S(   3,    2), S(  15,   -6), S(  12,   -3), S(  39,    1), S(  31,   -3), S(  28,    7),
		S( -10,    6), S(   0,    9), S(   6,    3), S(  19,   11), S(   8,    4), S(  12,    7), S(   0,    7), S(   2,    3),
		S(  -2,    4), S(  -9,    8), S(   3,    6), S(  10,    7), S(  11,    3), S(   1,    2), S(  -3,    5), S(  10,   -4),
		S(   3,    4), S(  14,    3), S(   5,    2), S(  10,    4), S(  11,    8), S(   8,    0), S(  12,   -4), S(  18,   -2),
		S(  25,    6), S(  10,  -11), S(  21,  -12), S(   4,   -2), S(  10,    0), S(  17,   -8), S(  28,  -10), S(  23,   -7),
		S(  13,   -4), S(  27,    4), S(  11,   -3), S(  -1,   -1), S(   6,   -5), S(   3,    7), S(  19,  -10), S(  30,  -15),
	},
	{
		S( -15,   20), S( -17,   22), S( -18,   29), S( -22,   25), S(  -7,   18), S(  15,   12), S(  13,   15), S(  28,   10),
		S( -13,   13), S(  -5,   19), S(   8,   19), S(  26,    7), S(  13,    8), S(  30,    2), S(  37,   -1), S(  39,   -5),
		S( -18,   11), S(  14,    6), S(   7,    7), S(  13,    1), S(  36,   -9), S(  54,  -18), S(  75,  -16), S(  38,  -15),
		S( -18,   14), S(  -2,    7), S(  -3,   12), S(   1,    6), S(   8,   -6), S(  19,  -12), S(  20,   -9), S(  12,  -12),
		S( -27,    9), S( -26,    9), S( -19,    8), S( -11,    5), S( -10,    2), S( -14,   -1), S(   8,  -10), S(  -8,  -10),
		S( -30,    6), S( -26,    1), S( -22,    0), S( -17,    1), S(  -9,   -4), S(  -3,  -13), S(  21,  -28), S(   3,  -24),
		S( -31,    0), S( -25,    1), S( -15,   -1), S( -13,   -3), S(  -7,  -10), S(  -2,  -15), S(   7,  -22), S( -22,  -12),
		S( -13,    5), S( -13,    0), S(  -9,    5), S(  -1,   -3), S(   5,   -9), S(   2,   -5), S(   0,   -8), S( -13,   -5),
	},
	{
		S( -23,   -1), S( -35,   12), S( -21,   34), S(   4,   26), S(  -6,   29), S(   5,   17), S(  66,  -42), S(  11,   -2),
		S( -12,  -13), S( -25,    0), S( -24,   35), S( -34,   54), S( -30,   68), S(   4,   29), S(  -4,    9), S(  36,    8),
		S(   2,  -13), S(  -3,   -4), S(  -8,   24), S(   0,   31), S(  12,   37), S(  25,    8), S(  50,  -13), S(  42,   -5),
		S( -10,   -1), S(  -6,   10), S(  -8,   17), S( -11,   33), S( -17,   34), S(   9,   22), S(  11,   32), S(  21,   10),
		S(  -3,   -5), S( -13,   19), S( -10,   22), S( -12,   26), S(  -7,   29), S(  -2,   24), S(   4,   15), S(  15,    8),
		S(  -6,  -18), S(  -3,    0), S( -11,    9), S(  -8,   14), S(  -5,   19), S(   0,    6), S(   9,  -11), S(  12,  -22),
		S(   1,  -26), S(  -5,  -29), S(   3,  -22), S(   5,  -16), S(   4,  -14), S(   9,  -39), S(  14,  -68), S(  28,  -84),
		S(  -9,  -33), S(  -4,  -28), S(  -1,  -24), S(   4,  -12), S(   3,  -31), S(  -9,  -35), S(  -1,  -51), S(   5,  -50),
	},
	{
		S(  45,  -58), S(  63,  -32), S( 118,  -38), S(  47,  -14), S(  60,  -34), S(   8,   -5), S(  34,    7), S( 122,  -76),
		S(-116,   29), S(   3,   26), S(  23,   13), S( 152,  -20), S(  95,  -12), S(  40,   27), S(  17,   42), S( -66,   45),
		S(-126,   27), S(  28,   21), S(  31,   11), S(  50,    0), S(  88,   -5), S(  96,   11), S(  -5,   41), S( -66,   34),
		S( -77,    6), S( -42,   16), S( -14,   12), S(  -5,    2), S( -17,   -3), S( -33,   18), S( -80,   31), S(-151,   31),
		S( -81,   -3), S( -44,    9), S( -39,   11), S( -33,    6), S( -46,    6), S( -53,   13), S( -86,   22), S(-150,   23),
		S( -37,   -5), S(   1,    4), S( -29,   10), S( -14,    5), S( -19,    6), S( -39,   15), S( -26,   12), S( -64,   12),
		S(  48,  -19), S(  28,   -1), S(  22,   -1), S(   8,   -6), S(  -1,   -1), S(   6,    4), S(  26,    0), S(  31,  -12),
		S(  35,  -45), S(  64,  -33), S(  45,  -21), S( -11,  -18), S(  28,  -32), S( -13,   -4), S(  46,  -23), S(  47,  -49),
	},
};

constexpr InitialParam MOBILITY[4][28] = {
	{S(-124, -105), S( -33,  -57), S( -11,  -29), S(  -2,   -6), S(   6,    5), S(   9,   16), S(  17,   21), S(  27,   24), S(  35,   23), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)},
	{S( -28, -157), S( -46,  -83), S( -18,  -37), S(  -8,  -15), S(   5,   -4), S(  12,    5), S(  19,   15), S(  25,   21), S(  29,   27), S(  34,   30), S(  36,   35), S(  48,   30), S(  51,   34), S(  62,   26), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)},
	{S( -42, -113), S( -35,  -84), S( -21,  -32), S( -13,  -16), S(  -7,   -8), S(  -5,   -1), S(  -3,    4), S(  -0,    8), S(   2,   10), S(   7,   12), S(  11,   15), S(  14,   18), S(  19,   19), S(  22,   20), S(  21,   20), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)},
	{S(   0,    0), S(   0,    0), S(-217,  -56), S( -17, -262), S( -43,  -92), S( -10,  -84), S(  -6,  -57), S(  -3,  -37), S(   1,  -22), S(   2,    1), S(   6,    5), S(  10,   10), S(  14,   16), S(  18,   15), S(  20,   17), S(  23,   22), S(  24,   23), S(  24,   28), S(  26,   30), S(  28,   29), S(  35,   28), S(  46,   11), S(  61,    5), S(  76,  -10), S(  82,  -12), S( 191,  -74), S(  85,  -32), S(  60,  -38)}
};

constexpr InitialParam THREATS[6][6] = {
	{S(  18,   -8), S(  39,    1), S(  43,   35), S(  62,  -16), S(  44,  -45), S(   0,    0)},
	{S(  -4,    8), S(   0,    0), S(  20,   21), S(  48,  -12), S(  22,  -44), S(   0,    0)},
	{S(   3,   13), S(  22,   21), S(   0,    0), S(  30,   -1), S(  35,   56), S(   0,    0)},
	{S(  -8,   11), S(   4,   16), S(  11,   10), S(   0,    0), S(  42,  -10), S(   0,    0)},
	{S(  -3,    7), S(   1,    9), S(  -2,   22), S(   2,   -5), S(   0,    0), S(   0,    0)},
	{S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)}
};

constexpr InitialParam PASSED_PAWN[8] = {S(   0,    0), S(  -3,    4), S(  -7,   13), S(  -8,   37), S(  11,   61), S(   6,  120), S(  48,  110), S(   0,    0)};
constexpr InitialParam ISOLATED_PAWN[8] = {S(  -0,    4), S(  -3,  -11), S( -11,   -5), S(  -7,  -12), S( -10,  -13), S(  -7,   -3), S(  -1,  -11), S(  -8,    9)};
constexpr InitialParam PAWN_PHALANX[8] = {S(   0,    0), S(   4,   -2), S(  13,    6), S(  22,   17), S(  49,   57), S( 129,  186), S(-215,  443), S(   0,    0)};
constexpr InitialParam DEFENDED_PAWN[8] = {S(   0,    0), S(   0,    0), S(  17,   11), S(  12,    8), S(  13,   15), S(  26,   36), S( 156,   28), S(   0,    0)};

constexpr InitialParam KNIGHT_OUTPOST = S(  31,   23);
constexpr InitialParam BISHOP_PAIR = S(  20,   59);
constexpr InitialParam ROOK_OPEN[2] = {S(  27,    9), S(  15,    8)};

constexpr InitialParam PAWN_STORM[3][8] = {
    {S(  23,  -37), S(  21,  -20), S(  19,   -5), S(  10,    1), S(   5,    3), S(   1,    7), S(   2,    5), S(  11,  -12)},
    {S(   0,    0), S( -49,  -33), S(  29,   -0), S(   1,    5), S(  -6,   11), S(  -7,   15), S(  -9,   17), S(   7,   -4)},
    {S(  -3,   -1), S(  -2,    6), S(   6,    9), S(   3,   10), S(   5,    9), S(   5,    9), S(   3,    8), S(  -9,   -3)}
};
constexpr InitialParam PAWN_SHIELD[3][8] = {
    {S(  -0,  -16), S(  -9,  -10), S(  -7,  -13), S(  -1,  -10), S(  11,  -10), S(  13,  -18), S(  20,  -19), S(  12,   -2)},
    {S(   0,    0), S( -18,   -1), S( -12,   -6), S(   5,   -1), S(  18,   -3), S(  28,  -15), S(  51,  -25), S(  20,    3)},
    {S(  -2,  -11), S(  -2,   -8), S(  -0,   -7), S(   1,   -1), S(   6,    2), S(   4,    6), S(  14,   11), S(  -7,    7)}
};
constexpr InitialParam SAFE_KNIGHT_CHECK = S(  81,   -6);
constexpr InitialParam SAFE_BISHOP_CHECK = S(  19,   -7);
constexpr InitialParam SAFE_ROOK_CHECK = S(  58,   -6);
constexpr InitialParam SAFE_QUEEN_CHECK = S(  35,   12);
constexpr InitialParam SAFETY_OFFSET = S(   0,    0);

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

    addEvalParam(params, KNIGHT_OUTPOST);
    addEvalParam(params, BISHOP_PAIR);
    addEvalParamArray(params, ROOK_OPEN);

    addEvalParamArray2D(params, PAWN_STORM);
    addEvalParamArray2D(params, PAWN_SHIELD);
    addEvalParam(params, SAFE_KNIGHT_CHECK);
    addEvalParam(params, SAFE_BISHOP_CHECK);
    addEvalParam(params, SAFE_ROOK_CHECK);
    addEvalParam(params, SAFE_QUEEN_CHECK);
    addEvalParam(params, SAFETY_OFFSET);
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
    // prevent king safety gradients from being zero
    params.back().mg = 20;
    params.back().eg = 20;
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

    state.ss << "constexpr InitialParam KNIGHT_OUTPOST = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam BISHOP_PAIR = ";
    printSingle<ALIGN_SIZE>(state);
    state.ss << ";\n";

    state.ss << "constexpr InitialParam ROOK_OPEN[2] = ";
    printArray<ALIGN_SIZE>(state, 2);
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

    state.ss << "constexpr InitialParam SAFETY_OFFSET = ";
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
