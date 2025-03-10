#include "sirius/defs.h"

#define S(mg, eg) {mg, eg}

constexpr PackedScore MATERIAL[6] = {S(  61,  128), S( 295,  433), S( 312,  444), S( 393,  787), S( 761, 1454), S(0, 0)};

constexpr PackedScore PSQT[6][64] = {
	{
		S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
		S(  49,   80), S(  26,   89), S(  52,   78), S(  58,   72), S(  65,   66), S(  35,   90), S(  27,  103), S(  57,   84),
		S(  11,    9), S(   4,   36), S(  23,   -8), S(  31,  -32), S(  17,  -26), S(   8,   -8), S( -15,   29), S(  -2,   14),
		S(   9,   10), S(   2,   15), S(   7,   -7), S(  13,  -23), S(   0,  -21), S(  -2,  -10), S(  -7,   13), S( -12,   11),
		S(   4,    2), S(  -2,   19), S(   5,   -9), S(  17,  -14), S(  11,  -13), S(  -2,   -8), S( -13,   14), S( -14,    2),
		S(  -1,    0), S(   9,   14), S(   4,   -1), S(   0,    2), S(  -5,   -1), S( -10,   -5), S( -20,   12), S( -25,    0),
		S(   8,    1), S(  28,    8), S(  37,  -19), S(   8,    6), S(  -5,   -2), S(  -5,   -5), S( -12,   17), S( -16,    4),
		S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
	},
	{
		S( -78,  -93), S( -78,  -25), S( -27,  -16), S(   8,   -9), S( -29,    4), S( -62,    4), S(-107,    9), S( -93,  -53),
		S( -12,  -15), S(  21,    0), S(  32,   -5), S(  13,    4), S(  13,   10), S(   2,   13), S(  -5,   18), S( -13,    3),
		S(  15,  -10), S(   8,    0), S(  52,  -10), S(  42,    8), S(  30,   15), S(  13,   14), S(   2,    7), S(  -9,    3),
		S(  34,    1), S(  34,    9), S(  47,   12), S(  38,   17), S(  30,   25), S(  30,   14), S(  19,    6), S(   7,   12),
		S(  17,   12), S(  20,   13), S(  30,   14), S(  25,   26), S(  25,   20), S(  19,   19), S(  19,    3), S(   4,    6),
		S(  12,   -2), S(  16,    0), S(  14,   -2), S(  11,   16), S(   4,   15), S(   0,    0), S(   1,   -4), S( -12,   -5),
		S(   4,   -1), S(  12,   -6), S(   5,   -6), S(   9,    0), S(   2,    0), S(  -8,   -3), S(  -9,   -6), S( -22,   -7),
		S( -11,  -17), S( -14,   -8), S(  -6,  -10), S(  -1,    2), S(  -7,   -2), S( -23,   -8), S( -17,  -13), S( -49,   -7),
	},
	{
		S( -46,  -13), S( -41,    8), S(  -9,   -1), S( -73,   11), S( -62,   17), S( -54,    5), S( -28,    8), S( -36,    9),
		S( -24,   -7), S( -45,    5), S( -10,    2), S(  -1,    0), S( -20,   10), S(   0,    0), S( -15,   -5), S(  -5,   -5),
		S(  13,   10), S(   7,    5), S(  28,    3), S(  12,    9), S(  21,    4), S(  -3,    0), S(   6,    8), S(   3,    3),
		S( -10,    5), S(   9,    9), S(   7,   14), S(  21,   17), S(  22,   23), S(  18,   10), S(   3,   13), S(  -9,    5),
		S(  22,   -8), S(   2,   10), S(  12,   10), S(  11,   15), S(  21,   13), S(   2,   18), S(   1,    8), S(   1,    0),
		S(  22,  -10), S(  24,   -7), S(   5,   -6), S(  12,   10), S(   2,    7), S(   1,   -2), S(  16,    3), S(   2,   -5),
		S(  27,  -21), S(  20,  -27), S(  25,   -9), S(   4,   -4), S(   1,   -1), S(  11,  -17), S(   2,  -24), S(  14,  -11),
		S(  26,  -39), S(  15,  -15), S(  -9,    0), S(  16,   -6), S(   1,   -4), S(   1,    1), S(  14,   -5), S(   5,  -18),
	},
	{
		S(  10,    9), S( -27,   35), S( -14,   25), S(  -8,   16), S( -27,   25), S( -21,   29), S( -12,   23), S(  -2,   19),
		S(  25,    2), S(  30,    7), S(  15,   12), S(   7,   12), S(  17,   15), S(   3,   27), S(  -1,   23), S(   3,   14),
		S(   4,    1), S(  29,    4), S(  19,    0), S(  25,   -1), S(   3,   10), S(   0,   18), S(  10,   15), S( -10,   18),
		S(  -2,    7), S(   1,   12), S(  10,    7), S(   8,    2), S(   2,   11), S(   6,   14), S(   3,   14), S( -10,   17),
		S(  -9,   -1), S(  11,    3), S(  -9,    6), S(   6,    2), S(  -4,    4), S(  -9,    8), S( -12,    9), S( -13,    7),
		S(   9,  -24), S(  24,  -27), S(   2,  -17), S(   3,  -16), S(  -9,   -8), S( -13,   -9), S( -11,  -10), S( -18,   -7),
		S(  -8,  -27), S(  24,  -36), S(   5,  -24), S(   4,  -21), S(  -3,  -18), S(  -4,  -15), S( -14,  -13), S( -17,  -18),
		S(  -6,  -35), S(  -5,  -16), S(  -1,  -17), S(   9,  -23), S(   1,  -21), S(  -6,  -14), S(  -6,  -18), S(  -8,  -15),
	},
	{
		S(  -6,  -10), S(  33,  -37), S( -10,   20), S(  -8,   11), S(  12,   -8), S( -23,   12), S( -36,    2), S( -30,   10),
		S(  40,   -2), S(   0,   11), S(  -8,   29), S( -33,   52), S( -21,   36), S(  -2,    6), S(  -2,   -8), S(   5,   -5),
		S(  18,    3), S(  17,    5), S(   8,   22), S(  -7,   22), S(   3,   14), S(   7,   12), S(   8,   -9), S(  17,   -9),
		S(   7,   14), S(   3,   32), S(   8,    8), S( -11,   30), S(  -7,   28), S(   9,    4), S(   6,   16), S(   3,    9),
		S(   8,    8), S(  12,    9), S(  -1,   13), S(   2,   24), S(  -6,   32), S(  -2,   21), S(   3,   16), S(   1,   10),
		S(  11,  -21), S(  12,  -11), S(   4,    3), S(  -6,   16), S(  -3,   12), S(  -3,   16), S(   9,   -5), S(  10,  -20),
		S(   5,  -41), S(  10,  -62), S(   8,  -38), S(   5,   -7), S(   4,   -4), S(   4,  -16), S(   3,  -20), S(   8,  -34),
		S(   6,  -44), S( -19,  -38), S( -20,  -20), S(  -4,  -16), S(  -4,   -9), S(  -6,  -14), S( -11,  -17), S( -11,  -14),
	},
	{
		S( -17,  -70), S( -14,   -9), S( -59,    6), S( -61,   18), S(   8,   -5), S(   8,   -5), S(   8,   -5), S(   8,   -5),
		S( -57,   17), S(  -7,   46), S( -24,   51), S(   0,   42), S(   8,   -5), S(   8,   -5), S(   8,   -5), S(   8,   -5),
		S(  13,    4), S(  42,   34), S(  35,   43), S(  -3,   59), S(   8,   -5), S(   8,   -5), S(   8,   -5), S(   8,   -5),
		S( -22,    1), S(  -4,   29), S( -10,   39), S( -46,   49), S(   8,   -5), S(   8,   -5), S(   8,   -5), S(   8,   -5),
		S( -45,   -8), S( -17,   14), S( -11,   20), S( -44,   33), S(   8,   -5), S(   8,   -5), S(   8,   -5), S(   8,   -5),
		S(  -8,  -24), S(  18,   -6), S(  -3,    2), S( -20,   14), S(   8,   -5), S(   8,   -5), S(   8,   -5), S(   8,   -5),
		S(  29,  -39), S(  32,  -17), S(  10,  -11), S( -17,    1), S(   8,   -5), S(   8,   -5), S(   8,   -5), S(   8,   -5),
		S(  22,  -70), S(  28,  -36), S(   3,  -23), S(  -3,  -24), S(   8,   -5), S(   8,   -5), S(   8,   -5), S(   8,   -5),
	},
};

constexpr PackedScore MOBILITY[4][28] = {
	{S( -14,  -29), S( -39,  -45), S( -17,  -15), S(  -9,    1), S(   2,    9), S(   6,   19), S(  14,   22), S(  23,   26), S(  34,   19), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)},
	{S( -21,  -46), S( -36,  -58), S( -23,  -29), S( -15,  -11), S(  -5,   -2), S(   1,    8), S(   4,   16), S(   8,   19), S(   9,   22), S(  12,   22), S(  13,   22), S(  18,   16), S(  15,   22), S(  22,    2), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)},
	{S( -23,  -46), S( -32,  -70), S( -16,  -47), S(  -3,  -26), S(  -2,  -12), S(  -3,   -3), S(  -2,    4), S(   1,   10), S(   4,   13), S(   8,   18), S(   8,   26), S(  10,   32), S(  14,   35), S(  20,   34), S(  29,   32), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)},
	{S( -27,    7), S( -61,  -78), S( -94, -106), S( -28, -203), S( -29,  -65), S( -23,   -5), S( -14,  -18), S( -11,    3), S( -11,   21), S(  -8,   30), S(  -6,   33), S(  -2,   34), S(  -2,   44), S(   2,   42), S(   2,   46), S(   4,   47), S(   4,   49), S(   7,   49), S(   5,   51), S(  13,   42), S(  19,   34), S(  27,   18), S(  31,   19), S(  42,   -1), S(  61,  -15), S(  51,  -16), S(  34,  -12), S(   3,  -39)}
};

constexpr PackedScore THREAT_BY_PAWN[6] = {S(   4,  -21), S(  66,   29), S(  63,   59), S(  77,   26), S(  73,   -9), S(   0,    0)};
constexpr PackedScore THREAT_BY_KNIGHT[2][6] = {
	{S(   7,   28), S(  23,   28), S(  40,   43), S(  73,   13), S(  56,  -29), S(   0,    0)},
	{S(  -7,    8), S(  11,   27), S(  29,   29), S(  63,   33), S(  57,   -2), S(   0,    0)}
};
constexpr PackedScore THREAT_BY_BISHOP[2][6] = {
	{S(  -2,   35), S(  39,   32), S( -11,   30), S(  68,   16), S(  68,   52), S(   0,    0)},
	{S(  -3,    6), S(  19,   22), S( -24,  -16), S(  44,   45), S(  45,  120), S(   0,    0)}
};
constexpr PackedScore THREAT_BY_ROOK[2][6] = {
	{S(   0,   42), S(  18,   59), S(  26,   55), S(   7,  -38), S(  75,   10), S(   0,    0)},
	{S(  -7,    6), S(   3,   14), S(  13,    1), S(   7,  -84), S(  64,   46), S(   0,    0)}
};
constexpr PackedScore THREAT_BY_QUEEN[2][6] = {
	{S(   5,    9), S(  24,   20), S(   8,   50), S(  19,   15), S(  10,  -51), S(  96,   40)},
	{S(  -2,    8), S(   0,    6), S(  -8,   17), S(  -5,    4), S( -17,  -77), S( 112,   59)}
};
constexpr PackedScore THREAT_BY_KING[6] = {S( -25,   50), S(   8,   48), S(  31,   40), S(  74,    7), S(   0,    0), S(   0,    0)};
constexpr PackedScore PUSH_THREAT = S(  14,   19);

constexpr PackedScore ISOLATED_PAWN[8] = {S(  -7,    5), S(  -5,  -15), S( -12,   -8), S( -10,  -16), S( -11,  -15), S(  -7,   -8), S(  -3,  -14), S(  -8,    6)};
constexpr PackedScore DOUBLED_PAWN[8] = {S(   0,  -60), S(  12,  -37), S(   2,  -28), S(   1,  -20), S(  -3,  -15), S(  -4,  -20), S(   8,  -42), S(   7,  -74)};
constexpr PackedScore BACKWARDS_PAWN[8] = {S(   0,    0), S(  -9,  -13), S(  -2,  -14), S(  -8,  -11), S(   0,  -15), S(  31,  -14), S(   0,    0), S(   0,    0)};
constexpr PackedScore PAWN_PHALANX[8] = {S(   0,    0), S(   5,   -5), S(  10,   -3), S(  20,    7), S(  42,   38), S( 103,  218), S(  -5,  340), S(   0,    0)};
constexpr PackedScore DEFENDED_PAWN[8] = {S(   0,    0), S(   0,    0), S(  19,    6), S(  11,    7), S(  16,   20), S(  33,   59), S( 152,   61), S(   0,    0)};
constexpr PackedScore CANDIDATE_PASSER[2][8] = {
	{S(   0,    0), S( -36,  -10), S( -18,   -8), S(   1,   27), S(  28,   54), S(  60,   89), S(   0,    0), S(   0,    0)},
	{S(   0,    0), S( -20,   -6), S(  -9,   14), S(  -5,   30), S(  20,   42), S(  30,  157), S(   0,    0), S(   0,    0)}
};

constexpr PackedScore PASSED_PAWN[2][2][8] = {
	{
		{S(   0,    0), S(   0,    0), S(   0,    0), S( -40,  -40), S( -16,   25), S(  -1,  155), S(  54,  222), S(   0,    0)},
		{S(   0,    0), S(   0,    0), S(   0,    0), S( -27,  -53), S(   7,  -23), S(  32,   21), S(  67,   28), S(   0,    0)}
	},
	{
		{S(   0,    0), S(   0,    0), S(   0,    0), S( -31,  -56), S(   0,  -23), S(  35,   11), S(  72,   -5), S(   0,    0)},
		{S(   0,    0), S(   0,    0), S(   0,    0), S( -31,  -62), S(   1,  -34), S(  10,   -8), S( -12,  -12), S(   0,    0)}
	}
};
constexpr PackedScore OUR_PASSER_PROXIMITY[8] = {S(   0,    0), S(  49,   83), S(  13,   66), S(   3,   43), S(   5,   32), S(  10,   25), S(  19,   22), S(  14,   21)};
constexpr PackedScore THEIR_PASSER_PROXIMITY[8] = {S(   0,    0), S( -49,    7), S(  20,    1), S(  17,   27), S(  21,   47), S(  16,   78), S(  18,   79), S(  19,   68)};

constexpr PackedScore PAWN_STORM[2][4][8] = {
	{
		{S(  16,    9), S(-107,  -74), S( -16,  -23), S(  20,  -29), S(   5,   -9), S(  -7,    3), S(  -7,    1), S(   0,    0)},
		{S(  16,   -6), S(  18, -139), S(  50,  -61), S(  22,  -27), S(   9,  -17), S( -11,   -1), S(  -6,   -1), S(   0,    0)},
		{S(   2,   -5), S(  77, -126), S(  72,  -55), S(  36,  -32), S(   7,   -9), S(  -9,    1), S( -13,    8), S(   0,    0)},
		{S(   7,  -11), S( 145, -103), S(  47,  -23), S(  20,   -1), S(  -3,    6), S( -11,   -1), S( -11,    4), S(   0,    0)}
	},
	{
		{S(   0,    0), S(   0,    0), S( -11,   37), S( -15,    0), S( -10,    3), S(  10,   -1), S(  -7,   -1), S(   0,    0)},
		{S(   0,    0), S(   0,    0), S(  38,   -6), S(  -9,   -1), S(  -4,    3), S(  -5,    6), S(  -6,   -2), S(   0,    0)},
		{S(   0,    0), S(   0,    0), S(  54,  -26), S(   1,  -18), S(   2,   -2), S(   7,    2), S(  -8,    4), S(   0,    0)},
		{S(   0,    0), S(   0,    0), S(   2,    5), S(  11,    7), S( -15,    3), S(  -7,   -3), S(  -3,   -7), S(   0,    0)}
	}
};
constexpr PackedScore PAWN_SHIELD[4][8] = {
	{S(  26,  -16), S( -17,   23), S( -17,    6), S(   1,   -3), S(   2,  -11), S( -35,  -48), S( -77,  -71), S(   0,    0)},
	{S(  31,  -16), S( -14,    0), S(  -8,   -1), S(  16,  -16), S(  20,  -30), S(   5,  -36), S( -25,  -68), S(   0,    0)},
	{S(   5,   -2), S(  -1,  -16), S(  -2,    1), S(  -4,   -4), S(  -4,   -8), S(  19,  -42), S(  19,  -85), S(   0,    0)},
	{S(  12,   -7), S(  -3,   -3), S(  -9,   -3), S(   1,   -6), S(   7,  -11), S(  34,  -30), S( -12,  -34), S(   0,    0)}
};
constexpr PackedScore SAFE_KNIGHT_CHECK = S(  90,    2);
constexpr PackedScore SAFE_BISHOP_CHECK = S(  17,   31);
constexpr PackedScore SAFE_ROOK_CHECK = S(  78,   17);
constexpr PackedScore SAFE_QUEEN_CHECK = S(  33,   44);
constexpr PackedScore UNSAFE_KNIGHT_CHECK = S(  12,    2);
constexpr PackedScore UNSAFE_BISHOP_CHECK = S(  19,   12);
constexpr PackedScore UNSAFE_ROOK_CHECK = S(  24,   -4);
constexpr PackedScore UNSAFE_QUEEN_CHECK = S(   5,   15);
constexpr PackedScore QUEENLESS_ATTACK = S( -79,   -2);
constexpr PackedScore KING_ATTACKS = S(   4,   -1);
constexpr PackedScore WEAK_KING_RING = S(   7,   -1);

constexpr int SAFETY_SCALE_ATTACKERS[6] = {66, 93, 128, 164, 199, 239};

constexpr PackedScore MINOR_BEHIND_PAWN = S(   6,   13);
constexpr PackedScore KNIGHT_OUTPOST = S(  21,   17);
constexpr PackedScore BISHOP_PAWNS[7] = {S(   3,   20), S(   6,   19), S(   4,   11), S(   2,    4), S(  -2,   -4), S(  -2,  -17), S(  -7,  -29)};
constexpr PackedScore BISHOP_PAIR = S(  22,   60);
constexpr PackedScore LONG_DIAG_BISHOP = S(  16,    9);
constexpr PackedScore ROOK_OPEN[2] = {S(  26,    4), S(  14,    4)};

constexpr PackedScore TEMPO = S(  32,   34);

constexpr PackedScore COMPLEXITY_PAWNS = S(   0,    9);
constexpr PackedScore COMPLEXITY_PASSERS = S(   0,    1);
constexpr PackedScore COMPLEXITY_PAWNS_BOTH_SIDES = S(   0,   65);
constexpr PackedScore COMPLEXITY_PAWN_ENDGAME = S(   0,   70);
constexpr PackedScore COMPLEXITY_OFFSET = S(   0, -128);

#undef S
