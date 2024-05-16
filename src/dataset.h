#pragma once

#include <fstream>
#include <span>
#include <vector>

enum class CoeffType : uint8_t
{
    LINEAR,
    SAFETY
};

struct Coefficient
{
    int16_t index;
    int16_t white;
    int16_t black;
    CoeffType type;
};

struct Position
{
    int coeffBegin;
    int coeffEnd;
    double wdl;
    double phase;
};

struct Dataset
{
    std::vector<Coefficient> allCoefficients;
    std::vector<Position> positions;
};

Dataset loadDataset(std::ifstream& file);
