#include <iostream>
#include <string>

#include "tune.h"
#include "eval_fn.h"

int main()
{
    std::string datasetFilepath;
    std::string outFilepath;
    std::cin >> datasetFilepath >> outFilepath;

    std::ifstream datasetFile(datasetFilepath);

    Dataset data = loadDataset(datasetFile);

    EvalParams params = tune(data);
    for (auto& param : params)
    {
        param.mg = std::round(param.mg);
        param.eg = std::round(param.eg);
    }
    EvalFn::printEvalParamsExtracted(params);
}
