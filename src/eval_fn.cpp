#include "eval_fn.h"

#include <sstream>
#include <iomanip>

using TraceElem = std::array<int, 2>;

struct Trace
{
    TraceElem psqt[6][64];
};

Trace getTrace(const chess::Board& board)
{
    Trace trace = {};
    
    for (int sq = 0; sq < 64; sq++)
    {
        chess::Piece pce = board.at(static_cast<chess::Square>(sq));
        if (pce == chess::Piece::NONE)
            continue;

        chess::PieceType type = chess::utils::typeOfPiece(pce);
        chess::Color color = chess::Board::color(pce);

        // flip if white
        int square = sq ^ (color == chess::Color::WHITE ? 0b111000 : 0);

        trace.psqt[static_cast<int>(type)][square][static_cast<int>(color)]++;
    }

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

std::pair<size_t, size_t> EvalFn::getCoefficients(const chess::Board& board)
{
    reset();
    size_t pos = m_Coefficients.size();
    Trace trace = getTrace(board);
    addCoefficientArray2D(trace.psqt);
    return {pos, m_Coefficients.size()};
}

struct InitialParams
{
    int psqt[6][64][2];
};

#define S(mg, eg) {mg, eg}

constexpr InitialParams DEFAULT_PARAMS = {
    {
        {
            S(  82,   94), S(  82,   94), S(  82,   94), S(  82,   94), S(  82,   94), S(  82,   94), S(  82,   94), S(  82,   94), 
            S( 101,  282), S( 106,  278), S(  88,  261), S( 123,  224), S( 140,  216), S( 128,  231), S(  78,  273), S(  37,  271), 
            S(  44,  206), S(  69,  200), S(  93,  185), S(  86,  165), S( 100,  153), S( 138,  140), S(  77,  178), S(  50,  185), 
            S(  62,  131), S(  81,  120), S(  75,  109), S(  87,   95), S(  99,   90), S(  86,   95), S(  81,  111), S(  47,  115), 
            S(  43,  111), S(  75,  103), S(  71,   87), S(  89,   87), S(  89,   83), S(  79,   86), S(  76,   97), S(  32,   98), 
            S(  51,  101), S(  72,  101), S(  76,   88), S(  71,   97), S(  88,   94), S(  66,   91), S( 108,   93), S(  63,   93), 
            S(  48,  113), S(  82,  104), S(  64,  108), S(  60,  104), S(  75,  104), S( 102,   98), S( 123,   96), S(  65,   93), 
            S(  82,   94), S(  82,   94), S(  82,   94), S(  82,   94), S(  82,   94), S(  82,   94), S(  82,   94), S(  82,   94), 
        },
        {
            S( 116,  250), S( 272,  220), S( 299,  248), S( 284,  255), S( 377,  233), S( 228,  244), S( 217,  242), S( 233,  177), 
            S( 269,  244), S( 313,  255), S( 411,  242), S( 363,  261), S( 418,  236), S( 409,  242), S( 349,  240), S( 345,  217), 
            S( 332,  247), S( 358,  259), S( 365,  285), S( 399,  281), S( 433,  254), S( 457,  259), S( 408,  246), S( 345,  244), 
            S( 352,  256), S( 353,  284), S( 355,  289), S( 389,  298), S( 361,  293), S( 385,  286), S( 343,  282), S( 368,  251), 
            S( 328,  262), S( 337,  275), S( 354,  293), S( 351,  296), S( 361,  297), S( 363,  282), S( 347,  266), S( 318,  264), 
            S( 320,  252), S( 334,  271), S( 354,  274), S( 352,  289), S( 360,  289), S( 350,  272), S( 359,  261), S( 317,  249), 
            S( 315,  245), S( 311,  253), S( 336,  259), S( 339,  276), S( 341,  273), S( 356,  260), S( 335,  254), S( 334,  238), 
            S( 284,  217), S( 318,  242), S( 290,  257), S( 309,  268), S( 311,  253), S( 324,  263), S( 315,  237), S( 265,  237), 
        },
        {
            S( 342,  277), S( 330,  268), S( 349,  270), S( 318,  290), S( 266,  293), S( 331,  288), S( 387,  265), S( 321,  285), 
            S( 339,  278), S( 386,  285), S( 361,  288), S( 369,  279), S( 386,  289), S( 428,  271), S( 377,  291), S( 400,  248), 
            S( 356,  292), S( 371,  296), S( 418,  288), S( 392,  304), S( 433,  282), S( 418,  301), S( 417,  285), S( 408,  277), 
            S( 341,  300), S( 365,  306), S( 390,  298), S( 397,  305), S( 395,  306), S( 377,  292), S( 376,  296), S( 363,  282), 
            S( 359,  282), S( 376,  292), S( 379,  305), S( 398,  301), S( 402,  299), S( 378,  314), S( 381,  281), S( 346,  277), 
            S( 389,  275), S( 387,  291), S( 386,  301), S( 379,  307), S( 383,  312), S( 389,  294), S( 379,  282), S( 388,  268), 
            S( 358,  281), S( 394,  279), S( 378,  281), S( 370,  302), S( 374,  301), S( 398,  282), S( 405,  282), S( 378,  265), 
            S( 375,  260), S( 351,  277), S( 365,  274), S( 360,  284), S( 349,  287), S( 351,  281), S( 368,  262), S( 360,  270), 
        },
        {
            S( 520,  519), S( 526,  515), S( 532,  516), S( 543,  508), S( 574,  498), S( 583,  491), S( 531,  501), S( 514,  507), 
            S( 485,  518), S( 481,  529), S( 515,  519), S( 537,  511), S( 512,  508), S( 557,  501), S( 565,  495), S( 542,  492), 
            S( 482,  515), S( 483,  517), S( 495,  512), S( 513,  507), S( 534,  497), S( 546,  495), S( 558,  494), S( 522,  495), 
            S( 452,  517), S( 472,  512), S( 496,  512), S( 505,  508), S( 489,  509), S( 499,  505), S( 513,  497), S( 500,  497), 
            S( 455,  510), S( 448,  516), S( 455,  517), S( 470,  516), S( 477,  511), S( 471,  507), S( 483,  496), S( 461,  493), 
            S( 448,  502), S( 459,  507), S( 469,  508), S( 479,  505), S( 477,  508), S( 472,  497), S( 494,  487), S( 470,  491), 
            S( 439,  506), S( 462,  505), S( 460,  512), S( 469,  511), S( 471,  506), S( 478,  503), S( 485,  498), S( 419,  511), 
            S( 464,  503), S( 468,  514), S( 482,  515), S( 487,  517), S( 491,  503), S( 477,  499), S( 437,  514), S( 455,  486), 
        },
        {
            S( 983,  944), S(1004,  953), S(1051,  944), S(1068,  953), S(1089,  935), S(1100,  935), S(1024,  935), S(1009,  968), 
            S( 998,  940), S( 996,  942), S( 995,  972), S( 978, 1009), S( 987, 1004), S(1118,  970), S(1049,  976), S(1158,  853), 
            S(1007,  915), S(1009,  939), S(1030,  968), S(1050,  950), S(1061,  993), S(1104,  940), S(1131,  911), S(1084,  913), 
            S( 995,  930), S(1007,  958), S(1014,  955), S(1016,  980), S(1021,  997), S(1028,  994), S(1012,  983), S(1038,  956), 
            S(1007,  933), S(1014,  954), S(1013,  955), S(1017,  982), S(1027,  967), S(1018,  969), S(1033,  957), S(1015,  939), 
            S(1008,  910), S(1030,  904), S(1017,  966), S(1024,  939), S(1019,  961), S(1032,  947), S(1035,  944), S(1020,  922), 
            S(1006,  933), S(1027,  933), S(1030,  925), S(1027,  942), S(1033,  931), S(1045,  900), S(1048,  889), S(1030,  901), 
            S(1026,  915), S(1009,  913), S(1017,  918), S(1033,  893), S(1013,  929), S(1000,  916), S( 973,  927), S(1016,  882), 
        },
        {
            S( 146,  -79), S(  77,  -32), S(  99,  -36), S(  95,  -29), S( 112,  -28), S( 100,  -16), S(  63,  -15), S(  64,  -44), 
            S(  47,  -25), S( 118,   -9), S(  77,  -10), S(  52,   -6), S(  60,   -2), S( 104,    9), S(   2,   23), S( -29,    9), 
            S(  51,  -22), S( 138,   -7), S(  29,    7), S(  58,    2), S(  -4,   15), S(  73,   17), S(  83,   21), S(  -9,   12), 
            S(  -4,  -25), S(   8,   -2), S(  37,    7), S(  -6,   17), S( -22,   20), S( -19,   26), S(   5,   19), S( -41,    7), 
            S(  48,  -42), S(  37,  -12), S(  14,    8), S( -50,   25), S( -45,   27), S( -69,   32), S( -19,   11), S( -74,    1), 
            S(   7,  -33), S(  23,   -9), S( -61,   18), S( -71,   28), S( -80,   33), S( -56,   26), S( -10,    9), S( -33,   -4), 
            S(  41,  -37), S( -13,   -7), S( -22,    7), S( -73,   24), S( -58,   22), S( -26,   15), S(  13,    0), S(  11,  -18), 
            S( -42,  -36), S(  30,  -35), S(   3,  -17), S( -60,  -10), S(   6,  -36), S( -34,  -13), S(  24,  -30), S(  20,  -57), 
        }
    }
};

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
    addEvalParamArray2D(params, DEFAULT_PARAMS.psqt);
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
    state.ss << "PSQT: {\n";
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
    state.ss << "}";
}

void EvalFn::printEvalParams(const EvalParams& params)
{
    PrintState state{params, 0};
    printPSQTs<0>(state);
    std::cout << state.ss.str() << std::endl;
}

void EvalFn::printEvalParamsExtracted(const EvalParams& params)
{
    PrintState state{params, 0};
    printPSQTs<4>(state);
}
