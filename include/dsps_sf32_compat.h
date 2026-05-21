#pragma once
// Stereo-Biquad-Implementierung als Ersatz für dsps_biquad_sf32.
// Die Library erwartet eine Stereo-Funktion (len=1 = 1 Stereo-Frame = 2 Floats).
// state[4]: w[0],w[1] = linker Kanal, w[2],w[3] = rechter Kanal.
// Standard-Mono dsps_biquad_f32 verarbeitet nur s[0] (Links) — Rechts bleibt ungefiltert.
static inline int dsps_biquad_sf32(float* x, float* y, int len, float* c, float* w) {
    const float b0 = c[0], b1 = c[1], b2 = c[2], a1 = c[3], a2 = c[4];
    for (int i = 0; i < len; i++) {
        float xl = x[i * 2];
        float xr = x[i * 2 + 1];
        float yl = b0 * xl + w[0];
        w[0] = b1 * xl - a1 * yl + w[1];
        w[1] = b2 * xl - a2 * yl;
        float yr = b0 * xr + w[2];
        w[2] = b1 * xr - a1 * yr + w[3];
        w[3] = b2 * xr - a2 * yr;
        y[i * 2]     = yl;
        y[i * 2 + 1] = yr;
    }
    return 0;
}
