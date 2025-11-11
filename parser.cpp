#include "parser.hpp"

#include <sstream>
#include <iostream>

std::optional<Vec3> parseVec3(const std::string &msg) {
    std::istringstream iss(msg);
    std::vector<double> found;
    std::string tok;
    while (iss >> tok) {
        try {
            size_t idx = 0;
            double v = std::stod(tok, &idx);
            // stod may parse prefixes; ensure at least one char parsed
            if (idx > 0) {
                found.push_back(v);
                if (found.size() >= 3) break;
            }
        } catch (const std::exception &) {
            // not a number token, continue
        }
    }
    if (found.size() < 3) return std::nullopt;
    return Vec3{found[0], found[1], found[2]};
}
