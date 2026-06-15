#pragma once
#include "engine/Renderer.h"
#include "engine/Color.h"
#include <string>

// Expressive character faces drawn from primitives (circles + lines), so they
// match the shape-based art and need no image assets. The face reacts to live
// game state -- the personality/comedy layer.
namespace haul {

enum class FaceState { Neutral, Greedy, Scared, Panic, Happy, Dead };

inline const char* faceLabel(FaceState s) {
    switch (s) {
        case FaceState::Neutral: return "...";
        case FaceState::Greedy:  return "GREEDY";
        case FaceState::Scared:  return "SCARED";
        case FaceState::Panic:   return "PANIC!";
        case FaceState::Happy:   return "CHA-CHING";
        case FaceState::Dead:    return "DEAD";
    }
    return "";
}

// A curved mouth made of short segments. sag > 0 => smile (U), < 0 => frown.
inline void drawMouth(eng::Renderer& r, float cx, float cy, float halfW, float sag,
                      eng::Color c, int thickness) {
    const int N = 8;
    float px = 0.f, py = 0.f;
    for (int i = 0; i <= N; ++i) {
        float t = -1.f + 2.f * i / N;
        float x = cx + t * halfW;
        float y = cy + sag * (1.f - t * t);
        if (i > 0) r.drawLine(px, py, x, y, c, thickness);
        px = x;
        py = y;
    }
}

inline void drawFace(eng::Renderer& r, FaceState s, float x, float y, float w, float h) {
    using eng::Color;
    const Color skin{248, 224, 180, 255};
    const Color dark{40, 30, 30, 255};
    const Color white{255, 255, 255, 255};
    const Color sweat{120, 180, 255, 255};

    const float cx = x + w * 0.5f;
    const float cy = y + h * 0.5f;
    r.fillCircle(cx, cy, w * 0.42f, skin);

    const float eyeY = cy - h * 0.06f;
    const float eyeDX = w * 0.18f;
    const float eyeR = w * 0.09f;

    auto eye = [&](float ex, float pupilR) {
        r.fillCircle(ex, eyeY, eyeR, white);
        r.fillCircle(ex, eyeY, pupilR, dark);
    };

    switch (s) {
        case FaceState::Neutral:
            eye(cx - eyeDX, eyeR * 0.5f);
            eye(cx + eyeDX, eyeR * 0.5f);
            drawMouth(r, cx, cy + h * 0.20f, w * 0.15f, 0.f, dark, 3);
            break;

        case FaceState::Greedy:
            eye(cx - eyeDX, eyeR * 0.6f);
            eye(cx + eyeDX, eyeR * 0.6f);
            drawMouth(r, cx, cy + h * 0.14f, w * 0.22f, h * 0.13f, dark, 4);  // big grin
            r.fillCircle(cx + eyeDX + eyeR * 1.2f, eyeY - eyeR, w * 0.035f,
                         Color{255, 220, 90, 255});  // gold sparkle
            break;

        case FaceState::Happy:
            // Closed, upturned eyes (^ ^) and a wide smile.
            drawMouth(r, cx - eyeDX, eyeY, w * 0.07f, -h * 0.05f, dark, 3);
            drawMouth(r, cx + eyeDX, eyeY, w * 0.07f, -h * 0.05f, dark, 3);
            drawMouth(r, cx, cy + h * 0.16f, w * 0.20f, h * 0.13f, dark, 4);
            break;

        case FaceState::Scared:
            eye(cx - eyeDX, eyeR * 0.4f);
            eye(cx + eyeDX, eyeR * 0.4f);
            // Worried brows (outer corners up).
            r.drawLine(cx - eyeDX - eyeR, eyeY - eyeR * 1.7f, cx - eyeDX + eyeR, eyeY - eyeR, dark, 2);
            r.drawLine(cx + eyeDX + eyeR, eyeY - eyeR * 1.7f, cx + eyeDX - eyeR, eyeY - eyeR, dark, 2);
            r.fillCircle(cx, cy + h * 0.22f, w * 0.05f, dark);            // small open mouth
            r.fillCircle(cx + w * 0.30f, cy - h * 0.04f, w * 0.035f, sweat);
            break;

        case FaceState::Panic:
            // Big whites, tiny pinprick pupils, brows up, gaping mouth, sweat.
            r.fillCircle(cx - eyeDX, eyeY, eyeR * 1.15f, white);
            r.fillCircle(cx + eyeDX, eyeY, eyeR * 1.15f, white);
            r.fillCircle(cx - eyeDX, eyeY, eyeR * 0.3f, dark);
            r.fillCircle(cx + eyeDX, eyeY, eyeR * 0.3f, dark);
            r.drawLine(cx - eyeDX - eyeR, eyeY - eyeR * 1.9f, cx - eyeDX + eyeR, eyeY - eyeR * 1.2f, dark, 2);
            r.drawLine(cx + eyeDX + eyeR, eyeY - eyeR * 1.9f, cx + eyeDX - eyeR, eyeY - eyeR * 1.2f, dark, 2);
            r.fillCircle(cx, cy + h * 0.24f, w * 0.10f, dark);
            r.fillCircle(cx + w * 0.32f, cy - h * 0.02f, w * 0.04f, sweat);
            r.fillCircle(cx - w * 0.33f, cy + h * 0.04f, w * 0.03f, sweat);
            break;

        case FaceState::Dead:
            // X eyes and a flat, queasy mouth.
            r.drawLine(cx - eyeDX - eyeR, eyeY - eyeR, cx - eyeDX + eyeR, eyeY + eyeR, dark, 3);
            r.drawLine(cx - eyeDX - eyeR, eyeY + eyeR, cx - eyeDX + eyeR, eyeY - eyeR, dark, 3);
            r.drawLine(cx + eyeDX - eyeR, eyeY - eyeR, cx + eyeDX + eyeR, eyeY + eyeR, dark, 3);
            r.drawLine(cx + eyeDX - eyeR, eyeY + eyeR, cx + eyeDX + eyeR, eyeY - eyeR, dark, 3);
            drawMouth(r, cx, cy + h * 0.22f, w * 0.15f, -h * 0.04f, dark, 3);
            break;
    }
}

} // namespace haul
