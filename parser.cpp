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

std::optional<double> parseDouble(const std::string &msg) {
    std::istringstream iss(msg);
    std::string tok;
    while (iss >> tok) {
        try {
            size_t idx = 0;
            double v = std::stod(tok, &idx);
            if (idx > 0) return v;
        } catch (const std::exception &) {
            std::cerr << "Failed to parse double from token: " << tok << std::endl;
            // not a number token, continue
        }
    }
    return std::nullopt;
}
