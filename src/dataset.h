#pragma once

#include <fstream>
#include <span>
#include <vector>

struct Coefficient
{
    int16_t index;
    int16_t value;
};

struct Position
{
    int coeffBegin;
    int coeffEnd;
    uint8_t wdl2;
    uint8_t phase24;

    double wdl() const
    {
        return static_cast<double>(wdl2) * 0.5;
    }

    double phase() const
    {
        return static_cast<double>(phase24) * (1.0 / 24.0);
    }
};

struct Dataset
{
    std::vector<Coefficient> allCoefficients;
    std::vector<Position> positions;
};

Dataset loadDataset(std::ifstream& file);
