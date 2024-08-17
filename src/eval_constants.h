#include "sirius/defs.h"

#define S(mg, eg) {mg, eg}

constexpr PackedScore MATERIAL[6] = {S(  66,   98), S( 312,  407), S( 337,  414), S( 421,  744), S( 863, 1371), S(0, 0)};

constexpr PackedScore PSQT[6][64] = {
	{
		S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
		S(  19,   97), S(  37,   93), S(  81,   96), S(  85,   68), S(  94,   54), S(  54,   68), S(  31,   90), S(  18,   84),
		S(  47,   25), S(  63,   43), S(  84,    6), S(  53,   -7), S(  42,   -5), S(  38,    0), S(   9,   36), S(   4,   25),
		S(  19,    3), S(  12,   21), S(  30,   -7), S(  18,   -9), S(   5,   -7), S(   0,   -2), S(  -8,   21), S( -11,   14),
		S(   7,  -10), S(   1,   16), S(  14,  -10), S(   8,   -6), S(   1,   -6), S(  -6,   -6), S( -18,   17), S( -18,   -1),
		S(  11,  -19), S(  13,    2), S(  -4,   -4), S(  -5,    1), S( -13,    1), S( -19,   -8), S( -28,    8), S( -32,   -6),
		S(  26,  -24), S(  45,   -2), S(  29,   -6), S(   1,   12), S( -16,    5), S( -14,   -1), S( -21,   16), S( -24,    0),
		S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
	},
	{
		S( -74,  -61), S( -67,  -17), S( -12,  -17), S(  18,  -11), S( -31,    6), S( -69,   13), S(-109,   18), S( -89,  -30),
		S(  -1,   -5), S(  28,   -1), S(  34,  -11), S(  12,   -1), S(  12,    6), S(  -1,    7), S(  -7,   21), S( -14,   13),
		S(  23,   -5), S(  16,   -5), S(  61,  -19), S(  36,   -2), S(  18,    3), S(  11,    3), S(   2,    5), S(  -6,    7),
		S(  39,    4), S(  39,    5), S(  48,    2), S(  37,   11), S(  33,   12), S(  29,    5), S(  17,    4), S(   7,   19),
		S(  11,   15), S(  23,    7), S(  28,    5), S(  23,   18), S(  26,   12), S(  19,   10), S(  21,    1), S(   0,   14),
		S(  -1,    1), S(  15,   -6), S(   4,  -13), S(  16,    6), S(   4,    8), S(  -5,   -6), S(  -5,   -5), S( -17,    1),
		S(   3,   13), S(   6,    0), S(   3,   -8), S(   1,   -4), S(   1,   -5), S( -11,   -6), S( -14,    1), S( -20,    4),
		S( -15,    9), S( -14,    5), S(  -3,   -7), S(  -5,    2), S( -11,   -3), S( -24,   -8), S( -16,    0), S( -54,   17),
	},
	{
		S( -40,   -7), S( -37,    4), S( -14,   -5), S( -78,   10), S( -75,   14), S( -59,    3), S( -34,    7), S( -15,   10),
		S( -30,   -3), S( -43,   10), S( -13,   -2), S(  -5,   -5), S( -24,    4), S(  -3,   -4), S(  -6,   -1), S( -10,   -3),
		S(  26,    9), S(  17,   -1), S(  48,    3), S(   8,    4), S(  15,   -6), S(   5,    4), S(   9,    2), S(   5,    5),
		S(  -1,    4), S(  13,    6), S(  10,   10), S(  21,   13), S(  19,   16), S(  13,    4), S(   4,   10), S(  -8,    7),
		S(  20,  -10), S(   5,    6), S(  10,    4), S(  12,   11), S(  18,    9), S(  -3,   12), S(  -1,    5), S(   0,    3),
		S(  18,   -9), S(  21,  -11), S(  11,   -4), S(   5,    7), S(  -4,    5), S(   5,    3), S(   9,    2), S(  -1,   -5),
		S(  25,  -14), S(  31,  -19), S(  20,  -14), S(   3,   -5), S(  -5,   -2), S(   9,  -19), S(   6,  -16), S(  17,   -6),
		S(  35,  -30), S(  20,  -12), S(  -4,    4), S(  13,   -8), S(  -1,   -6), S(   3,    4), S(  12,    1), S(  11,   -7),
	},
	{
		S(  13,    8), S( -20,   28), S(  -4,   24), S( -16,   18), S( -39,   31), S( -33,   32), S( -17,   24), S( -16,   24),
		S(  36,   -3), S(  33,    6), S(  22,    9), S(   4,   12), S(  19,   12), S(   0,   25), S(  -3,   21), S(  -5,   17),
		S(  18,   -4), S(  53,   -5), S(  36,   -5), S(  24,    3), S(   4,   10), S(   2,   18), S(  14,   13), S( -11,   19),
		S(  10,    2), S(  11,    7), S(  22,    2), S(  13,    3), S(   5,   10), S(   8,   14), S(   7,   13), S( -10,   19),
		S(  -6,   -2), S(  17,    1), S(   0,    4), S(   8,    2), S(  -4,    5), S(  -8,    8), S( -11,    8), S( -15,    9),
		S(   7,  -24), S(  18,  -25), S(   2,  -16), S(   0,  -14), S( -12,   -6), S( -16,  -10), S( -15,  -10), S( -21,   -6),
		S( -11,  -27), S(  17,  -34), S(   5,  -24), S(   0,  -21), S(  -6,  -18), S(  -7,  -16), S( -16,  -12), S( -20,  -15),
		S( -20,  -21), S( -13,  -11), S(  -2,  -17), S(   3,  -25), S(  -4,  -22), S( -11,  -14), S( -10,  -18), S( -11,  -11),
	},
	{
		S(   4,  -21), S(  56,  -55), S(  -4,   11), S( -15,   11), S(   5,   -9), S( -33,   16), S( -53,   12), S( -28,    1),
		S(  45,   -9), S(  -1,    5), S(  -3,   22), S( -40,   54), S( -31,   38), S( -12,   12), S( -12,   -6), S(   1,   -7),
		S(  16,    0), S(  20,   -4), S(   6,   18), S(  -7,   27), S(  -1,   22), S(   4,   15), S(   3,   -4), S(  15,   -7),
		S(  17,   12), S(  14,   32), S(  10,   14), S( -10,   34), S(  -7,   32), S(   6,    6), S(   6,   12), S(   4,    7),
		S(  16,    9), S(  22,   12), S(   8,   22), S(   3,   28), S(  -4,   30), S(  -2,   20), S(   3,   18), S(   0,    9),
		S(  15,  -20), S(  16,   -7), S(   6,    5), S(  -6,   21), S(  -6,   14), S(  -6,   16), S(   3,   -1), S(   3,  -10),
		S(  14,  -52), S(  18,  -67), S(   9,  -41), S(   5,   -7), S(   2,   -1), S(   4,  -18), S(   3,  -22), S(   9,  -34),
		S(   3,  -46), S( -14,  -46), S( -18,  -22), S(  -7,  -20), S(  -7,  -13), S( -11,   -8), S( -15,  -15), S( -14,  -12),
	},
	{
		S(  84,  -67), S(  47,  -12), S(  28,   -9), S(  38,  -17), S(   0,   -2), S(   0,   -2), S(   0,   -2), S(   0,   -2),
		S( -78,   38), S(  29,   35), S(  30,   29), S( 113,   -7), S(   0,   -2), S(   0,   -2), S(   0,   -2), S(   0,   -2),
		S( -55,   26), S(  43,   24), S(  73,   13), S(  52,    8), S(   0,   -2), S(   0,   -2), S(   0,   -2), S(   0,   -2),
		S( -97,   20), S( -30,   18), S( -21,   17), S( -22,    5), S(   0,   -2), S(   0,   -2), S(   0,   -2), S(   0,   -2),
		S(-120,   15), S( -48,   13), S( -26,    8), S( -35,    3), S(   0,   -2), S(   0,   -2), S(   0,   -2), S(   0,   -2),
		S( -62,    7), S(  -3,    1), S( -15,    3), S(  -8,   -2), S(   0,   -2), S(   0,   -2), S(   0,   -2), S(   0,   -2),
		S(   9,   -9), S(  27,   -5), S(   7,   -1), S(  -5,   -5), S(   0,   -2), S(   0,   -2), S(   0,   -2), S(   0,   -2),
		S(  10,  -38), S(  16,  -14), S( -15,    0), S(  -9,  -17), S(   0,   -2), S(   0,   -2), S(   0,   -2), S(   0,   -2),
	},
};

constexpr PackedScore MOBILITY[4][28] = {
	{S( -12,  -14), S( -42,  -69), S( -18,  -35), S(  -8,   -8), S(   3,    6), S(   6,   19), S(  15,   26), S(  24,   34), S(  33,   33), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)},
	{S( -20,  -28), S( -55,  -93), S( -32,  -39), S( -25,  -16), S( -12,   -4), S(  -5,    6), S(   0,   17), S(   6,   21), S(   8,   26), S(  14,   26), S(  15,   29), S(  30,   18), S(  30,   22), S(  53,    5), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)},
	{S( -31,  -37), S(  14,  -75), S( -26,  -45), S( -16,  -29), S( -10,  -23), S(  -4,   -7), S(   0,   -2), S(  -5,    9), S(   0,   12), S(   4,   18), S(   8,   23), S(  10,   31), S(  12,   37), S(  18,   40), S(  26,   34), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)},
	{S( -43,   13), S( -60,  -68), S(-110,  -46), S( -82, -240), S( -82,  -93), S( -36,  -34), S( -26,  -13), S( -18,   -9), S( -10,   -8), S(  -9,   18), S(  -7,   27), S(  -4,   37), S(  -1,   43), S(   3,   45), S(   6,   49), S(   8,   51), S(  10,   52), S(  10,   56), S(  11,   57), S(  12,   58), S(  20,   49), S(  28,   33), S(  32,   31), S(  60,   -1), S(  69,   -7), S( 104,  -42), S(  87,  -41), S(  40,  -44)}
};

constexpr PackedScore THREAT_BY_PAWN[6] = {S(   8,  -17), S(  67,   32), S(  68,   63), S(  83,   27), S(  77,  -13), S(   0,    0)};
constexpr PackedScore THREAT_BY_KNIGHT[2][6] = {
	{S(   7,   30), S(  33,   29), S(  39,   42), S(  80,    6), S(  46,  -26), S(   0,    0)},
	{S(  -4,   11), S(  15,   47), S(  32,   34), S(  66,   31), S(  56,   -4), S(   0,    0)}
};
constexpr PackedScore THREAT_BY_BISHOP[2][6] = {
	{S(   5,   37), S(  51,   27), S(  -5,   33), S(  77,    9), S(  73,   49), S(   0,    0)},
	{S(   1,   10), S(  23,   25), S( -20,    6), S(  46,   44), S(  51,  132), S(   0,    0)}
};
constexpr PackedScore THREAT_BY_ROOK[2][6] = {
	{S(   0,   46), S(  30,   56), S(  30,   54), S(  17,  -47), S(  79,   -1), S(   0,    0)},
	{S(  -7,   11), S(   6,   19), S(  18,    6), S(   8,  -76), S(  63,   52), S(   0,    0)}
};
constexpr PackedScore THREAT_BY_QUEEN[2][6] = {
	{S(  12,    7), S(  37,   13), S(  19,   48), S(  23,   -4), S(  11,  -64), S( 106,    6)},
	{S(   0,   13), S(   2,    9), S(  -4,   23), S(  -5,    3), S( -21,  -66), S( 109,   54)}
};
constexpr PackedScore THREAT_BY_KING[6] = {S( -15,   43), S(  29,   43), S(  24,   45), S( 108,    4), S(   0,    0), S(   0,    0)};
constexpr PackedScore PUSH_THREAT = S(  20,   18);

constexpr PackedScore ISOLATED_PAWN[8] = {S(  -6,    7), S(  -2,  -14), S( -11,   -6), S(  -9,  -14), S( -10,  -12), S(  -6,   -4), S(  -1,  -11), S( -11,    9)};
constexpr PackedScore PAWN_PHALANX[8] = {S(   0,    0), S(   4,   -2), S(  14,    5), S(  20,   16), S(  48,   58), S( 126,  190), S( -62,  399), S(   0,    0)};
constexpr PackedScore DEFENDED_PAWN[8] = {S(   0,    0), S(   0,    0), S(  20,   10), S(  14,    9), S(  15,   18), S(  25,   54), S( 159,   49), S(   0,    0)};

constexpr PackedScore PASSED_PAWN[2][2][8] = {
	{
		{S(   0,    0), S(   0,    0), S(   0,    0), S( -31,  -39), S( -17,   23), S( -37,  148), S(   1,  229), S(   0,    0)},
		{S(   0,    0), S(   0,    0), S(   0,    0), S( -19,  -51), S(  13,  -25), S(  15,   11), S(  82,   25), S(   0,    0)}
	},
	{
		{S(   0,    0), S(   0,    0), S(   0,    0), S( -28,  -57), S(  -2,  -23), S(  14,    1), S(  80,  -11), S(   0,    0)},
		{S(   0,    0), S(   0,    0), S(   0,    0), S( -31,  -62), S(  -2,  -35), S(  -7,  -25), S(   7,  -49), S(   0,    0)}
	}
};
constexpr PackedScore OUR_PASSER_PROXIMITY[8] = {S(   0,    0), S(  15,   89), S( -10,   66), S(   1,   39), S(   5,   27), S(  10,   19), S(  19,   20), S(  12,   19)};
constexpr PackedScore THEIR_PASSER_PROXIMITY[8] = {S(   0,    0), S( -67,   17), S(  20,   -2), S(  10,   26), S(  15,   47), S(   9,   78), S(  16,   78), S(  14,   68)};

constexpr PackedScore PAWN_STORM[3][8] = {
	{S(  51,  -34), S(  25,  -21), S(  17,   -4), S(   8,   -1), S(   1,    3), S(  -3,    8), S(  -3,    8), S(   5,   -8)},
	{S(   0,    0), S(  21,  -29), S(  21,    1), S(   4,   -1), S(  -5,    4), S(  -8,   12), S(  -8,   12), S(   3,   -3)},
	{S(   3,   -1), S(  -5,    5), S(   4,   10), S(   4,   10), S(   4,   10), S(   4,   12), S(   5,   11), S( -10,    1)}
};
constexpr PackedScore PAWN_SHIELD[3][8] = {
	{S(  14,  -13), S(   0,   -7), S(  -4,   -8), S(  -6,   -5), S(   6,   -7), S(  21,  -20), S(  36,  -16), S(  -9,   14)},
	{S(   0,    0), S(  -8,    0), S( -11,   -1), S(  -1,   -1), S(  12,   -5), S(  33,  -23), S(  56,  -27), S(   6,   12)},
	{S(  -6,   -3), S(  -5,   -1), S(  -1,   -2), S(   0,    2), S(   3,    3), S(   3,    0), S(  14,   -3), S(  -8,   17)}
};
constexpr PackedScore SAFE_KNIGHT_CHECK = S(  84,   -3);
constexpr PackedScore SAFE_BISHOP_CHECK = S(  24,   10);
constexpr PackedScore SAFE_ROOK_CHECK = S(  65,    0);
constexpr PackedScore SAFE_QUEEN_CHECK = S(  24,   17);
constexpr PackedScore UNSAFE_KNIGHT_CHECK = S(   5,    2);
constexpr PackedScore UNSAFE_BISHOP_CHECK = S(  12,   11);
constexpr PackedScore UNSAFE_ROOK_CHECK = S(  20,   -1);
constexpr PackedScore UNSAFE_QUEEN_CHECK = S(   6,   11);
constexpr PackedScore KING_ATTACKER_WEIGHT[4] = {S(  19,   -1), S(  15,    2), S(  16,  -14), S(   5,   -2)};
constexpr PackedScore KING_ATTACKS[14] = {S( -56,   32), S( -63,   27), S( -70,   23), S( -74,   26), S( -71,   22), S( -61,   16), S( -42,    8), S( -19,   -3), S(  21,  -23), S(  47,  -27), S(  82,  -45), S(  94,  -17), S( 128, -102), S(  84,   54)};

constexpr PackedScore KNIGHT_OUTPOST = S(  27,   19);
constexpr PackedScore BISHOP_PAWNS[7] = {S(   8,   23), S(   9,   19), S(   6,   11), S(   2,    4), S(  -4,   -5), S(  -6,  -18), S( -13,  -32)};
constexpr PackedScore BISHOP_PAIR = S(  22,   63);
constexpr PackedScore ROOK_OPEN[2] = {S(  25,    6), S(  16,    5)};

constexpr PackedScore TEMPO = S(  32,   34);

constexpr PackedScore COMPLEXITY_PAWNS = S(   0,    0);
constexpr PackedScore COMPLEXITY_PASSERS = S(   0,    0);
constexpr PackedScore COMPLEXITY_OFFSET = S(   0,    0);

#undef S
