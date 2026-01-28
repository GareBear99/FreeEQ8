#pragma once
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Biquad
{
    // Direct Form I
    double b0=1, b1=0, b2=0, a1=0, a2=0;
    double z1L=0, z2L=0, z1R=0, z2R=0;

    void reset()
    {
        z1L = z2L = z1R = z2R = 0.0;
    }

    inline float processL(float x)
    {
        const double y = b0 * x + z1L;
        z1L = b1 * x - a1 * y + z2L;
        z2L = b2 * x - a2 * y;
        return (float)y;
    }

    inline float processR(float x)
    {
        const double y = b0 * x + z1R;
        z1R = b1 * x - a1 * y + z2R;
        z2R = b2 * x - a2 * y;
        return (float)y;
    }

    enum class Type { Bell, LowShelf, HighShelf, HighPass, LowPass };

    // RBJ cookbook coefficients
    void set(Type type, double sampleRate, double freqHz, double Q, double gainDb)
    {
        freqHz = std::clamp(freqHz, 10.0, sampleRate * 0.45);
        Q = std::clamp(Q, 0.1, 24.0);

        const double A  = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * M_PI * (freqHz / sampleRate);
        const double cosw0 = std::cos(w0);
        const double sinw0 = std::sin(w0);
        const double alpha = sinw0 / (2.0 * Q);

        double b0_, b1_, b2_, a0_, a1_, a2_;

        switch (type)
        {
            case Type::Bell:
            {
                b0_ = 1.0 + alpha * A;
                b1_ = -2.0 * cosw0;
                b2_ = 1.0 - alpha * A;
                a0_ = 1.0 + alpha / A;
                a1_ = -2.0 * cosw0;
                a2_ = 1.0 - alpha / A;
            } break;

            case Type::LowPass:
            {
                b0_ = (1.0 - cosw0) * 0.5;
                b1_ = 1.0 - cosw0;
                b2_ = (1.0 - cosw0) * 0.5;
                a0_ = 1.0 + alpha;
                a1_ = -2.0 * cosw0;
                a2_ = 1.0 - alpha;
            } break;

            case Type::HighPass:
            {
                b0_ = (1.0 + cosw0) * 0.5;
                b1_ = -(1.0 + cosw0);
                b2_ = (1.0 + cosw0) * 0.5;
                a0_ = 1.0 + alpha;
                a1_ = -2.0 * cosw0;
                a2_ = 1.0 - alpha;
            } break;

            case Type::LowShelf:
            {
                // Shelf with slope proxy (S) from Q
                const double S = std::clamp(Q / 2.0, 0.1, 4.0);
                const double alphaS = sinw0/2.0 * std::sqrt((A + 1.0/A) * (1.0/S - 1.0) + 2.0);

                b0_ =    A*((A+1) - (A-1)*cosw0 + 2*std::sqrt(A)*alphaS);
                b1_ =  2*A*((A-1) - (A+1)*cosw0);
                b2_ =    A*((A+1) - (A-1)*cosw0 - 2*std::sqrt(A)*alphaS);
                a0_ =        (A+1) + (A-1)*cosw0 + 2*std::sqrt(A)*alphaS;
                a1_ =   -2*((A-1) + (A+1)*cosw0);
                a2_ =        (A+1) + (A-1)*cosw0 - 2*std::sqrt(A)*alphaS;
            } break;

            case Type::HighShelf:
            {
                const double S = std::clamp(Q / 2.0, 0.1, 4.0);
                const double alphaS = sinw0/2.0 * std::sqrt((A + 1.0/A) * (1.0/S - 1.0) + 2.0);

                b0_ =    A*((A+1) + (A-1)*cosw0 + 2*std::sqrt(A)*alphaS);
                b1_ = -2*A*((A-1) + (A+1)*cosw0);
                b2_ =    A*((A+1) + (A-1)*cosw0 - 2*std::sqrt(A)*alphaS);
                a0_ =        (A+1) - (A-1)*cosw0 + 2*std::sqrt(A)*alphaS;
                a1_ =    2*((A-1) - (A+1)*cosw0);
                a2_ =        (A+1) - (A-1)*cosw0 - 2*std::sqrt(A)*alphaS;
            } break;
        }

        // normalize
        b0 = b0_ / a0_;
        b1 = b1_ / a0_;
        b2 = b2_ / a0_;
        a1 = a1_ / a0_;
        a2 = a2_ / a0_;
    }
};
