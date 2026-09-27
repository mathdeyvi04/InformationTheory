#pragma once

#include <cstdint>
#include <queue>
#include <fstream>
#include <array>
#include <vector>
#include <functional>
#include <limits>
#include <utility>
#include <set>
#include <cmath>

class CompressionAlgorithm {
public:
    virtual ~CompressionAlgorithm() = default;
    virtual std::vector<uint8_t> apply(const std::vector<uint8_t>& data) = 0;
    virtual std::vector<uint8_t> deapply(const std::vector<uint8_t>& data) = 0;
};