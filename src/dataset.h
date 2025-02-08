#pragma once

#include <fstream>
#include <span>
#include <vector>
#include <array>

struct Coefficient
{
    int16_t index;
    int16_t white;
    int16_t black;
};

struct Position
{
    int coeffBegin;
    int coeffEnd;
    double wdl;
    double phase;
    double egScale;
    std::array<uint8_t, 2> safetyScales;
};

struct Dataset
{
    std::vector<Coefficient> allCoefficients;
    std::vector<Position> positions;
};

Dataset loadDataset(std::ifstream& file);
