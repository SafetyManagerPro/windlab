#pragma once
// Realtime-safe three-band stereo EQ: RBJ low shelf, bell, high shelf.
// Biquad coefficients smoothly interpolate (no heap allocation in process()).
#include <algorithm>
#include <array>
#include <cmath>

namespace windlab
{
    class ToneEQ
    {
    public:
        void prepare(double rate)
        {
            sampleRate = std::max(8000.0, rate);
            for (auto& band : bands)
                for (auto& channel : band)
                    channel.reset();
            lastBass = lastMid = lastTreble = 999.0f;
            setGains(0.0f, 0.0f, 0.0f);
        }

        void setGains(float bassDb, float midDb, float trebleDb)
        {
            bassDb = std::clamp(bassDb, -12.0f, 12.0f);
            midDb = std::clamp(midDb, -12.0f, 12.0f);
            trebleDb = std::clamp(trebleDb, -12.0f, 12.0f);
            if (bassDb != lastBass) { setBand(0, lowShelf, 180.0, bassDb); lastBass = bassDb; }
            if (midDb != lastMid) { setBand(1, bell, 1250.0, midDb); lastMid = midDb; }
            if (trebleDb != lastTreble) { setBand(2, highShelf, 5200.0, trebleDb); lastTreble = trebleDb; }
        }

        float process(int channel, float input)
        {
            const auto index = static_cast<std::size_t>(channel == 0 ? 0 : 1);
            for (auto& band : bands)
                input = band[index].tick(input);
            return input;
        }

    private:
        enum FilterType { lowShelf, bell, highShelf };
        struct Coeffs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
        struct Biquad
        {
            Coeffs now, target;
            double z1 = 0, z2 = 0;
            void reset() { now = target = {}; z1 = z2 = 0; }
            void set(const Coeffs& c) { target = c; }
            float tick(float sample)
            {
                constexpr double smooth = .0025; // approx 8ms at 48k
                now.b0 += smooth * (target.b0 - now.b0);
                now.b1 += smooth * (target.b1 - now.b1);
                now.b2 += smooth * (target.b2 - now.b2);
                now.a1 += smooth * (target.a1 - now.a1);
                now.a2 += smooth * (target.a2 - now.a2);
                const double x = static_cast<double>(sample);
                const double y = now.b0 * x + z1;
                z1 = now.b1 * x - now.a1 * y + z2;
                z2 = now.b2 * x - now.a2 * y;
                if (std::abs(z1) < 1e-22) z1 = 0;
                if (std::abs(z2) < 1e-22) z2 = 0;
                return static_cast<float>(y);
            }
        };

        void setBand(std::size_t bandIndex, FilterType type, double freq, double db)
        {
            constexpr double pi = 3.14159265358979323846;
            const double w = 2.0 * pi * std::min(freq, sampleRate * .40) / sampleRate;
            const double sn = std::sin(w), cs = std::cos(w);
            const double A = std::pow(10.0, db / 40.0);
            double b0, b1, b2, a0, a1, a2;
            if (type == bell)
            {
                const double alpha = sn / (2.0 * .85);
                b0 = 1.0 + alpha * A;
                b1 = -2.0 * cs;
                b2 = 1.0 - alpha * A;
                a0 = 1.0 + alpha / A;
                a1 = -2.0 * cs;
                a2 = 1.0 - alpha / A;
            }
            else
            {
                const double beta = 2.0 * std::sqrt(A) * sn / std::sqrt(2.0);
                if (type == lowShelf)
                {
                    b0 = A * ((A + 1) - (A - 1) * cs + beta);
                    b1 = 2 * A * ((A - 1) - (A + 1) * cs);
                    b2 = A * ((A + 1) - (A - 1) * cs - beta);
                    a0 = (A + 1) + (A - 1) * cs + beta;
                    a1 = -2 * ((A - 1) + (A + 1) * cs);
                    a2 = (A + 1) + (A - 1) * cs - beta;
                }
                else
                {
                    b0 = A * ((A + 1) + (A - 1) * cs + beta);
                    b1 = -2 * A * ((A - 1) + (A + 1) * cs);
                    b2 = A * ((A + 1) + (A - 1) * cs - beta);
                    a0 = (A + 1) - (A - 1) * cs + beta;
                    a1 = 2 * ((A - 1) - (A + 1) * cs);
                    a2 = (A + 1) - (A - 1) * cs - beta;
                }
            }
            const Coeffs c { b0/a0, b1/a0, b2/a0, a1/a0, a2/a0 };
            for (auto& channel : bands[bandIndex]) channel.set(c);
        }

        double sampleRate = 44100.0;
        float lastBass = 999.0f, lastMid = 999.0f, lastTreble = 999.0f;
        std::array<std::array<Biquad, 2>, 3> bands{};
    };
}
