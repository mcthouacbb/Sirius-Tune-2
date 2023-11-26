#include "dataset.h"
#include "chess.hpp"
#include "eval_fn.h"

#include <string>


constexpr struct {const char* str; double wdl;} wdls[] = {
    {"1-0", 1.0},
    {"0-1", 0.0},
    {"1/2-1/2", 0.5},
    {"1.0", 1.0},
    {"0.0", 0.0},
    {"0.5", 0.5}
};

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
                wdlResult  = wdl.wdl;
                break;
            }
        }
        if (wdlResult == -1.0)
        {
            std::cout << "Warning: line with no wdl marker, defaulting to 0.5" << std::endl;
            wdlResult = 0.5;
        }

        // will overflow to 0
        size_t fourthSpace = SIZE_MAX;
        for (int i = 0; i < 4; i++)
        {
            fourthSpace = line.find(' ', fourthSpace + 1);
            if (fourthSpace == std::string::npos)
            {
                std::cout << "Error: Invalid data: " << line << std::endl;
                exit(1);
            }
        }

        chess::Board board;
        board.setFen(std::string_view(line.begin(), line.begin() + fourthSpace));

        auto [coeffBegin, coeffEnd] = eval.getCoefficients(board);

        Position pos;
        pos.coeffBegin = coeffBegin;
        pos.coeffEnd = coeffEnd;
        pos.wdl = wdlResult;
        pos.phase =
            4 * chess::builtin::popcount(board.pieces(chess::PieceType::QUEEN)) +
            2 * chess::builtin::popcount(board.pieces(chess::PieceType::ROOK)) +
            chess::builtin::popcount(board.pieces(chess::PieceType::BISHOP)) +
            chess::builtin::popcount(board.pieces(chess::PieceType::KNIGHT));
        pos.phase /= 24.0;
        
        positions.push_back(pos);
    }
    return {allCoefficients, positions};
}
