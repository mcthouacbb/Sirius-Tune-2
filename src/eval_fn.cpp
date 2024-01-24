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

        chess::PieceType type = pce.type();
        chess::Color color = pce.color();

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
            S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
            S( 136,  282), S( 158,  274), S( 136,  275), S( 165,  226), S( 149,  222), S( 132,  233), S(  66,  278), S(  41,  292),
            S(  58,  223), S(  72,  231), S( 104,  197), S( 110,  176), S( 113,  168), S( 134,  153), S( 114,  200), S(  72,  198),
            S(  43,  155), S(  67,  144), S(  71,  124), S(  73,  116), S(  94,  107), S(  85,  110), S(  89,  129), S(  66,  129),
            S(  33,  130), S(  60,  127), S(  59,  110), S(  75,  107), S(  75,  105), S(  67,  107), S(  76,  118), S(  54,  111),
            S(  31,  124), S(  56,  126), S(  56,  109), S(  57,  121), S(  71,  113), S(  60,  111), S(  90,  116), S(  61,  107),
            S(  32,  129), S(  56,  130), S(  52,  116), S(  42,  122), S(  62,  127), S(  76,  116), S(  99,  115), S(  53,  108),
            S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
        },
        {
            S( 125,  262), S( 157,  322), S( 221,  336), S( 253,  328), S( 284,  331), S( 228,  308), S( 175,  329), S( 180,  240),
            S( 256,  315), S( 273,  336), S( 298,  344), S( 315,  343), S( 299,  336), S( 360,  321), S( 272,  332), S( 295,  300),
            S( 271,  331), S( 304,  346), S( 321,  362), S( 333,  363), S( 369,  348), S( 370,  343), S( 327,  336), S( 296,  321),
            S( 267,  342), S( 280,  363), S( 304,  374), S( 324,  376), S( 307,  377), S( 331,  371), S( 290,  362), S( 300,  335),
            S( 254,  343), S( 269,  352), S( 283,  376), S( 284,  376), S( 293,  379), S( 288,  369), S( 287,  355), S( 264,  335),
            S( 235,  328), S( 257,  346), S( 271,  356), S( 274,  369), S( 285,  368), S( 275,  352), S( 278,  341), S( 251,  330),
            S( 222,  319), S( 234,  335), S( 250,  343), S( 262,  347), S( 263,  346), S( 265,  341), S( 252,  326), S( 249,  329),
            S( 180,  312), S( 233,  299), S( 220,  328), S( 234,  331), S( 238,  332), S( 251,  322), S( 235,  306), S( 209,  300),
        },
        {
            S( 275,  351), S( 257,  362), S( 268,  359), S( 227,  373), S( 238,  366), S( 256,  357), S( 285,  350), S( 248,  348),
            S( 292,  338), S( 315,  357), S( 309,  361), S( 292,  364), S( 320,  355), S( 320,  353), S( 312,  361), S( 301,  336),
            S( 301,  367), S( 324,  361), S( 325,  372), S( 348,  361), S( 334,  366), S( 365,  367), S( 342,  360), S( 330,  360),
            S( 293,  362), S( 307,  378), S( 328,  373), S( 338,  386), S( 335,  379), S( 331,  376), S( 307,  375), S( 293,  362),
            S( 287,  358), S( 299,  375), S( 305,  383), S( 325,  379), S( 322,  379), S( 308,  378), S( 300,  373), S( 295,  348),
            S( 297,  358), S( 304,  368), S( 304,  376), S( 307,  375), S( 308,  379), S( 303,  376), S( 305,  359), S( 309,  348),
            S( 299,  352), S( 300,  353), S( 311,  351), S( 290,  366), S( 297,  368), S( 310,  356), S( 316,  358), S( 303,  333),
            S( 278,  336), S( 297,  353), S( 282,  334), S( 273,  355), S( 278,  352), S( 277,  352), S( 301,  338), S( 288,  323),
        },
        {
            S( 410,  640), S( 402,  647), S( 409,  656), S( 414,  652), S( 432,  643), S( 448,  633), S( 431,  635), S( 450,  630),
            S( 392,  640), S( 391,  652), S( 410,  656), S( 430,  647), S( 416,  647), S( 444,  633), S( 431,  629), S( 461,  616),
            S( 372,  640), S( 392,  643), S( 394,  645), S( 397,  643), S( 425,  630), S( 426,  624), S( 463,  615), S( 441,  611),
            S( 356,  642), S( 369,  641), S( 372,  650), S( 380,  646), S( 386,  631), S( 386,  626), S( 394,  622), S( 397,  616),
            S( 338,  635), S( 340,  640), S( 350,  642), S( 362,  641), S( 362,  637), S( 347,  635), S( 370,  622), S( 362,  617),
            S( 331,  631), S( 340,  631), S( 348,  630), S( 348,  635), S( 353,  631), S( 351,  623), S( 384,  603), S( 363,  604),
            S( 328,  626), S( 340,  630), S( 355,  630), S( 351,  632), S( 356,  624), S( 357,  620), S( 374,  611), S( 345,  616),
            S( 347,  621), S( 348,  631), S( 357,  638), S( 363,  637), S( 367,  629), S( 357,  624), S( 371,  620), S( 348,  613),
        },
        {
            S( 733, 1199), S( 741, 1212), S( 771, 1229), S( 804, 1215), S( 804, 1212), S( 809, 1205), S( 827, 1162), S( 774, 1193),
            S( 770, 1164), S( 748, 1206), S( 755, 1240), S( 748, 1257), S( 753, 1275), S( 790, 1234), S( 770, 1218), S( 813, 1194),
            S( 770, 1175), S( 768, 1192), S( 766, 1234), S( 782, 1235), S( 787, 1249), S( 828, 1229), S( 829, 1193), S( 826, 1180),
            S( 754, 1185), S( 758, 1208), S( 762, 1222), S( 761, 1246), S( 763, 1258), S( 776, 1244), S( 775, 1230), S( 782, 1209),
            S( 756, 1182), S( 754, 1211), S( 752, 1220), S( 761, 1239), S( 760, 1238), S( 759, 1229), S( 770, 1209), S( 773, 1196),
            S( 753, 1171), S( 760, 1187), S( 755, 1210), S( 754, 1208), S( 757, 1212), S( 764, 1203), S( 776, 1181), S( 770, 1169),
            S( 751, 1166), S( 756, 1170), S( 767, 1167), S( 766, 1177), S( 764, 1180), S( 773, 1154), S( 779, 1126), S( 790, 1096),
            S( 749, 1159), S( 739, 1167), S( 746, 1171), S( 761, 1162), S( 753, 1166), S( 740, 1165), S( 762, 1135), S( 755, 1135),
        },
        {
            S(  64, -103), S(  40,  -53), S(  73,  -44), S( -68,    6), S( -12,  -14), S(  38,  -11), S(  87,  -19), S( 194, -126),
            S( -53,  -11), S( -14,   18), S( -57,   31), S(  50,   12), S(  -3,   33), S(   3,   45), S(  42,   34), S(  21,    4),
            S( -74,    5), S(  28,   23), S( -39,   42), S( -58,   53), S( -17,   52), S(  58,   44), S(  38,   43), S(   2,   14),
            S( -42,   -5), S( -52,   29), S( -68,   46), S(-112,   59), S( -99,   58), S( -62,   53), S( -62,   44), S( -85,   19),
            S( -36,  -17), S( -45,   14), S( -75,   37), S(-102,   52), S( -99,   51), S( -63,   38), S( -67,   27), S( -90,   10),
            S(   8,  -27), S(  23,   -4), S( -33,   17), S( -45,   29), S( -39,   28), S( -37,   20), S(   9,    0), S(  -8,  -12),
            S(  95,  -49), S(  55,  -22), S(  41,   -9), S(   7,    1), S(   6,    5), S(  24,   -5), S(  71,  -23), S(  80,  -41),
            S(  91,  -83), S( 114,  -64), S(  88,  -45), S( -11,  -27), S(  53,  -52), S(  14,  -29), S(  95,  -55), S(  96,  -83),
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

void printMaterial(PrintState& state)
{
    state.ss << "Material: {";
    for (int j = 0; j < 5; j++)
    {
        printSingle<4>(state);
        state.ss << ", ";
    }
    state.ss << "}\n";
}

void EvalFn::printEvalParams(const EvalParams& params)
{
    PrintState state{params, 0};
    printPSQTs<0>(state);
    std::cout << state.ss.str() << std::endl;
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

void EvalFn::printEvalParamsExtracted(const EvalParams& params)
{
    PrintState state{extractMaterial(params), 0};
    printMaterial(state);
    printPSQTs<4>(state);
    std::cout << state.ss.str() << std::endl;
}
