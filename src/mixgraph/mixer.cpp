// mostly lifted from Tables.cpp, by Olivier and Ericus
#include "stdafx.h"

#include "mixer.h"
#include "../pervasives/pervasives.h"

using namespace modplug::pervasives;

namespace modplug {
namespace mixgraph {

double upsample_sinc_table[SINC_SIZE];
double downsample_sinc_table[SINC_SIZE];

// see https://ccrma.stanford.edu/~jos/sasp/Kaiser_Window.html
// see http://mathworld.wolfram.com/ModifiedBesselFunctionoftheFirstKind.html
// izero is an approximation of the zero-order modified
// bessel function of the first kind
// [(x / 2)^2k / k!^2] / [(x / 2)^2(k - 1) / (k - 1)!^2] = x^2 / (2k)^2
double izero(double x) {
    static const double tolerance = 1e-21; // lifted from jos
    double i_0 = 1;
    double term = 1;
    double denom = 0;
    do {
        denom += 2;
        term *= (x * x) / (denom * denom);
        i_0 += term;
    } while (term > tolerance * i_0);
    return i_0;
}

// see https://ccrma.stanford.edu/~jos/sasp/Kaiser_Window.html
double kaiser(double window_length, double beta, double n) {
    double det = (n / (window_length / 2));
    return izero(beta * sqrt(1 - det * det)) / izero(beta);
}

double normalized_sinc(double x) {
    if (x == 0) return 1;
    return sin(PI * x) / (PI * x);
}

static void prefill_sinc_fir(sample_t *sinc_table,
                             const double kaiser_beta,
                             const double lowpass_factor,
                             const int phases_per_tap,
                             const int taps)
{
    const double izero_beta = izero(kaiser_beta);
    const double kPi = 4.0 * atan(1.0) * lowpass_factor;
    const int midpoint = (taps / 2) * phases_per_tap;
    for (int isrc = 0; isrc < taps * phases_per_tap; isrc++) {
        double fsinc;
        int ix = (taps - 1) - (isrc & (taps - 1));
        ix = (ix * phases_per_tap) + (isrc / taps);

        if (ix == midpoint) {
            fsinc = 1.0;
        } else {
            double x = (double)(ix - midpoint) * (double)(1.0 / phases_per_tap);
            int det = (taps * taps) / 4;
            fsinc = sin(x * kPi)
                  * izero(kaiser_beta * sqrt(1 - x * x * (1.0 / det)))
                  / (izero_beta * x * kPi); // Kaiser window
        }
        *sinc_table++ = static_cast<sample_t>(fsinc * lowpass_factor);
    }
}

void init_tables() {
    prefill_sinc_fir(upsample_sinc_table,   8.5,    1.0, TAP_PHASES, FIR_TAPS);
    prefill_sinc_fir(downsample_sinc_table, 9.6377, 0.5, TAP_PHASES, FIR_TAPS);
}


}
}