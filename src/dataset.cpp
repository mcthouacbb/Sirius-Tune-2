#include "dataset.h"
#include "eval_fn.h"
#include "sirius/board.h"

#include <string>

constexpr struct
{
    const char* str;
    double wdl;
} wdls[] = {{"1-0", 1.0}, {"0-1", 0.0}, {"1/2-1/2", 0.5}, {"1.0", 1.0}, {"0.0", 0.0}, {"0.5", 0.5}};

Dataset loadDataset(std::ifstream& file)
{
    std::vector<Coefficient> allCoefficients;
    std::vector<Position> positions;
    EvalFn eval(allCoefficients);
    std::string line;

    while (std::getline(file, line))
    {
        double wdlResult = -1.0;
        for (auto& wdl : wdls)
        {
            if (line.find(wdl.str) != std::string::npos)
            {
                wdlResult = wdl.wdl;
                break;
            }
        }
        if (wdlResult == -1.0)
        {
            std::cout << "Warning: line with no wdl marker, defaulting to 0.5" << std::endl;
            wdlResult = 0.5;
        }

        // will overflow to 0
        size_t sixthSpace = SIZE_MAX;
        for (int i = 0; i < 6; i++)
        {
            sixthSpace = line.find(' ', sixthSpace + 1);
            if (sixthSpace == std::string::npos)
            {
                std::cout << "Error: Invalid data: " << line << std::endl;
                exit(1);
            }
        }

        Board board;
        board.setToFen(std::string_view(line.begin(), line.begin() + sixthSpace));

        auto [coeffBegin, coeffEnd, egScale] = eval.getCoefficients(board);

        Position pos;
        pos.coeffBegin = coeffBegin;
        pos.coeffEnd = coeffEnd;
        pos.wdl = wdlResult;
        pos.phase = 4 * board.pieces(PieceType::QUEEN).popcount()
            + 2 * board.pieces(PieceType::ROOK).popcount()
            + board.pieces(PieceType::BISHOP).popcount() + board.pieces(PieceType::KNIGHT).popcount();
        pos.phase /= 24.0;
        pos.egScale = egScale;

        positions.push_back(pos);

        if (positions.size() % 65536 == 0)
            std::cout << "Loaded " << positions.size() << " positions " << std::endl;
    }
    return {allCoefficients, positions};
}
