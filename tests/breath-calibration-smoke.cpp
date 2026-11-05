#include "Dsp/BreathCalibration.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    using windlab::calibrateBreath;
    assert(calibrateBreath(0.0f, 0.0f, 1.0f) == 0.0f);
    assert(std::abs(calibrateBreath(0.5f, 0.0f, 1.0f) - 0.5f) < 0.00001f);
    assert(calibrateBreath(1.0f, 0.0f, 1.0f) == 1.0f);
    assert(calibrateBreath(0.1f, 0.2f, 1.0f) == 0.0f);
    assert(std::abs(calibrateBreath(0.6f, 0.2f, 1.0f) - 0.5f) < 0.00001f);
    assert(calibrateBreath(0.5f, 0.0f, 2.0f) < 0.5f);
    assert(calibrateBreath(0.5f, 0.0f, 0.5f) > 0.5f);
    assert(calibrateBreath(-1.0f, 0.0f, 1.0f) == 0.0f);
    assert(calibrateBreath(2.0f, 0.0f, 1.0f) == 1.0f);
    std::cout << "WindLab breath calibration: PASS\n";
}
