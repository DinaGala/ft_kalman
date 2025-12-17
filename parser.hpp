#pragma once

#include <string>
#include <vector>
#include <optional>

struct Vec3 {
    double x;
    double y;
    double z;
};

struct ImuMeasurement {
    double timestamp;
    Vec3 acceleration;
    Vec3 direction;   // Euler angles (rad)
};

struct GpsMeasurement {
    double timestamp;
    Vec3 position;
};

struct InitMeasurement {
    double timestamp;
    Vec3 position;
    double speed;     // m/s
};

// Parse whitespace separated doubles from a message. Returns optional Vec3 if
// at least three doubles available. Additional values are ignored.
std::optional<Vec3> parseVec3(const std::string &msg);

// Parse a single double value from a message. Returns optional<double> if a
// numeric token is found.
std::optional<double> parseDouble(const std::string &msg);
