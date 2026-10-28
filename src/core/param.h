#pragma once
#include <cstdint>

enum class Curve : uint8_t { Lin, Log, Sqr, Stepped };
enum class Unit : uint8_t { None, Hz, Ms, Percent, Semi, Cents, Oct, Enum, Volts, Bpm, Deg, Db, Ratio, Int };

struct ParamSpec {
    const char* key;    // stable lowercase id
    const char* label;  // <= 3 chars for cells, e.g. "CUT"
    float min, max, def;
    Curve curve;
    Unit unit;
    const char* const* enumNames;
};

float paramToNorm(const ParamSpec&, float value);   // 0..1, clamps
float paramFromNorm(const ParamSpec&, float norm);  // clamps; Stepped rounds to integer steps
// key0to11: top-row column. 0..8 -> norm (col+1)/10, 9 -> 1, 10 -> min, 11 -> def.
// Stepped with <= 10 values: col c<10 -> min+c (clamped), 10 -> min, 11 -> def.
float paramCoarse(const ParamSpec&, int key0to11);
float paramFine(const ParamSpec&, float value, int dir);  // +-1/128 norm; Stepped: +-1 step
float paramClamp(const ParamSpec&, float value);          // Stepped: also rounds; NaN -> def
void paramFormat(const ParamSpec&, float value, char* out, int len, bool longForm);

extern const ParamSpec kAmountSpec;  // output amount, -1..+1
