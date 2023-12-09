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
    ThreadPool threadPool(4);

    std::vector<EvalParam> defaultParams = EvalFn::getInitialParams();
    double kValue = findKValue(data.positions, data.allCoefficients, defaultParams);
    std::cout << "K Value: " << kValue << std::endl;
    std::cout << calcError(threadPool, data.positions, data.allCoefficients, kValue, defaultParams) << std::endl;
    std::vector<Gradient> gradients(defaultParams.size());
    computeGradient(threadPool, data.positions, data.allCoefficients, 0.05, defaultParams, gradients);
    EvalFn::printEvalParams(gradients);
    EvalFn::printEvalParamsExtracted(defaultParams);
    // for (int i = 0; i < gradients.size(); i++)
        // std::cout << "{" << gradients[i].mg << ' ' << gradients[i].eg << "}, ";
    std::cout << "\n\n";

    tune(data, EvalParams(defaultParams.size(), {0, 0}), kValue);
}
