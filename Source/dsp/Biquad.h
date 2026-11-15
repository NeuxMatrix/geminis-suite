#pragma once
#include <cmath>

namespace geminis
{
/** Filtro de segundo orden (fórmulas RBJ), forma transpuesta II. */
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void reset() noexcept { z1 = z2 = 0.0f; }

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setLowpass (double sr, double hz, double q = 0.7071)   { set (sr, hz, q, 0.0, 0); }
    void setHighpass (double sr, double hz, double q = 0.7071)  { set (sr, hz, q, 0.0, 1); }
    void setPeak (double sr, double hz, double q, double dB)    { set (sr, hz, q, dB, 2); }
    void setLowShelf (double sr, double hz, double dB)          { set (sr, hz, 0.7071, dB, 3); }
    void setHighShelf (double sr, double hz, double dB)         { set (sr, hz, 0.7071, dB, 4); }

private:
    void set (double sr, double hz, double q, double dB, int type)
    {
        const double pi = 3.14159265358979323846;
        hz = std::fmin (std::fmax (hz, 10.0), sr * 0.45);
        const double w = 2.0 * pi * hz / sr;
        const double cs = std::cos (w), sn = std::sin (w);
        const double alpha = sn / (2.0 * q);
        const double A = std::pow (10.0, dB / 40.0);
        double B0 = 1, B1 = 0, B2 = 0, A0 = 1, A1 = 0, A2 = 0;

        switch (type)
        {
            case 0: B0 = (1 - cs) / 2; B1 = 1 - cs; B2 = (1 - cs) / 2; A0 = 1 + alpha; A1 = -2 * cs; A2 = 1 - alpha; break;
            case 1: B0 = (1 + cs) / 2; B1 = -(1 + cs); B2 = (1 + cs) / 2; A0 = 1 + alpha; A1 = -2 * cs; A2 = 1 - alpha; break;
            case 2: B0 = 1 + alpha * A; B1 = -2 * cs; B2 = 1 - alpha * A; A0 = 1 + alpha / A; A1 = -2 * cs; A2 = 1 - alpha / A; break;
            case 3:
            {
                const double s = 2.0 * std::sqrt (A) * alpha;
                B0 = A * ((A + 1) - (A - 1) * cs + s); B1 = 2 * A * ((A - 1) - (A + 1) * cs); B2 = A * ((A + 1) - (A - 1) * cs - s);
                A0 = (A + 1) + (A - 1) * cs + s;       A1 = -2 * ((A - 1) + (A + 1) * cs);    A2 = (A + 1) + (A - 1) * cs - s;
                break;
            }
            default:
            {
                const double s = 2.0 * std::sqrt (A) * alpha;
                B0 = A * ((A + 1) + (A - 1) * cs + s); B1 = -2 * A * ((A - 1) + (A + 1) * cs); B2 = A * ((A + 1) + (A - 1) * cs - s);
                A0 = (A + 1) - (A - 1) * cs + s;       A1 = 2 * ((A - 1) - (A + 1) * cs);      A2 = (A + 1) - (A - 1) * cs - s;
                break;
            }
        }

        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
};

inline float dbToGain (float dB) noexcept { return std::pow (10.0f, dB * 0.05f); }
} // namespace geminis
