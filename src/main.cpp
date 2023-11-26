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

    std::vector<EvalParam> defaultParams = EvalFn::getInitialParams();
    std::cout << calcError(data.positions, data.allCoefficients, 3.423, defaultParams) << std::endl;
}
