#pragma once

#include <fstream>
#include <span>
#include <vector>

struct Coefficient
{
    int16_t index;
    int8_t white;
    int8_t black;
};

struct Position
{
    std::span<const Coefficient> coefficients;
    double wdl;
    double phase;
};

struct Dataset
{
    std::vector<Coefficient> allCoefficients;
    std::vector<Position> positions;
};

Dataset loadDataset(std::ifstream& file);
