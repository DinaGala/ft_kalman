#pragma once

#include <string>
#include <vector>
#include <optional>

struct Vec3 {
    double x;
    double y;
    double z;
};

// Parse whitespace separated doubles from a message. Returns optional Vec3 if
// at least three doubles available. Additional values are ignored.
std::optional<Vec3> parseVec3(const std::string &msg);
