#include <iostream>
#include <string>

#include "eval_fn.h"
#include "sirius/attacks.h"
#include "sirius/zobrist.h"
#include "tune.h"


int main()
{
    attacks::init();

    std::string mode;
    std::cin >> mode;

    if (mode == "tune")
    {
        std::string datasetFilepath;
        std::string outFilepath;
        std::cin >> datasetFilepath >> outFilepath;

        std::ifstream datasetFile(datasetFilepath);
        std::ofstream outFile(outFilepath);

        Dataset data = loadDataset(datasetFile);

        EvalParams params = tune(data, outFile);
        EvalFn::printEvalParamsExtracted(params, std::cout);
        EvalFn::printEvalParamsExtracted(params, outFile);
    }
    else if (mode == "params")
    {
        EvalFn::printEvalParamsExtracted(EvalFn::getInitialParams(), std::cout);
    }
}
