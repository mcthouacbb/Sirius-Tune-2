#pragma once

#include "sirius/defs.h"

#include <array>
#include <fstream>
#include <span>
#include <vector>

struct Coefficient
{
    i16 index;
    i16 white;
    i16 black;
};

struct Position
{
    i32 coeffBegin;
    i32 coeffEnd;
    i32 score;
    double wdl;
    double phase;
    double egScale;
};

struct Dataset
{
    std::vector<Coefficient> allCoefficients;
    std::vector<Position> positions;
};

Dataset loadDataset(std::ifstream& file);
