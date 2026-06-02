// ════════════════════════════════════════════════════════════════
//  kira_faces.h — Project Cortex character faces
//  Bold graphic style
// ════════════════════════════════════════════════════════════════
#pragma once
#include <Avatar.h>
using namespace m5avatar;

// ══════════════════════════════════════════════════════════════
//  BigEye — large, round, graphic eye
// ══════════════════════════════════════════════════════════════
class BigEye : public Eye {
    uint16_t _iris;
public:
    BigEye(bool isLeft, uint16_t irisColor)
        : Eye(28, isLeft), _iris(irisColor) {}

    void draw(M5Canvas *spi, BoundingRect rect, DrawContext *ctx) override {
        Expression exp = ctx->getExpression();

        int cx = rect.getCenterX();
        int cy = rect.getCenterY();
        int RW = 28;

        uint16_t white  = spi->color565(255, 255, 255);
        uint16_t dark   = spi->color565(15,  10,  25);
        uint16_t shine1 = spi->color565(255, 255, 255);
        uint16_t shine2 = spi->color565(200, 220, 255);

        if (exp == Expression::Sleepy) {
            // Closed — flat line
            spi->fillRoundRect(cx-RW, cy-3, RW*2, 7, 3, dark);
            return;
        }

        if (exp == Expression::Happy) {
            // Happy ^^ — dark arc with shine, clearly visible on light bg
            uint16_t arc = spi->color565(40, 30, 80); // dark purple arc
            // Draw arc by drawing full circle then masking bottom half
            spi->fillCircle(cx, cy-4, RW, arc);
            spi->fillRect(cx-RW-2, cy-4, RW*2+4, RW+6, spi->color565(250,248,255));
            // Shine dot on top
            spi->fillCircle(cx-8, cy-16, 5, white);
            spi->fillCircle(cx-8, cy-16, 3, spi->color565(200,200,255));
            return;
        }

        if (exp == Expression::Sad) {
            // Sad — drooped
            spi->fillEllipse(cx, cy+4, RW, RW, white);
            spi->fillCircle(cx, cy+4, (int)(RW*0.52f), _iris);
            spi->fillCircle(cx, cy+4, (int)(RW*0.24f), dark);
            spi->fillCircle(cx-7, cy, 5, shine1);
            return;
        }

        if (exp == Expression::Doubt) {
            // Sceptical — slightly narrowed
            spi->fillEllipse(cx, cy, RW, (int)(RW*0.7f), white);
            spi->fillCircle(cx, cy, (int)(RW*0.52f), _iris);
            spi->fillCircle(cx, cy, (int)(RW*0.24f), dark);
            spi->fillCircle(cx-7, cy-5, 5, shine1);
            // Narrowed top lid
            spi->fillRect(cx-RW, cy-RW, RW*2, (int)(RW*0.55f), spi->color565(0,0,0));
            return;
        }

        // ── Normal open eye ──────────────────────────────────
        spi->fillEllipse(cx, cy, RW, RW, white);
        spi->fillCircle(cx, cy, (int)(RW*0.58f), _iris);
        spi->fillCircle(cx, cy, (int)(RW*0.28f), dark);
        spi->fillCircle(cx-8, cy-8, 6, shine1);
        spi->fillCircle(cx+6, cy-4, 3, shine2);
        // Top eyelid
        spi->fillRect(cx-RW, cy-RW, RW*2, (int)(RW*0.28f), spi->color565(0,0,0));
    }
};

// ══════════════════════════════════════════════════════════════
//  BoldMouth — clean graphic mouth
// ══════════════════════════════════════════════════════════════
// Global mouth ratio — set by mouthFn in main.cpp
float gMouthRatio = 0.0f;

class BoldMouth : public Mouth {
public:
    BoldMouth() : Mouth(0, 60, 0, 20) {}

    void draw(M5Canvas *spi, BoundingRect rect, DrawContext *ctx) override {
        Expression exp  = ctx->getExpression();
        float      open = gMouthRatio;   // 0.0 = closed, ~0.8 = wide open
        int cx = rect.getCenterX();
        int cy = rect.getCenterY();

        uint16_t dark   = spi->color565(30,  15,  10);
        uint16_t lip    = spi->color565(210, 100,  80);
        uint16_t teeth  = spi->color565(245, 235, 225);
        uint16_t tongue = spi->color565(220,  80,  80);

        // ── Speaking animation overrides expression ───────────────────────
        if (open > 0.15f) {  // higher threshold prevents flicker
            int mw = 20;
            int mh = (int)(open * 16);  // less wide, less tall
            spi->fillEllipse(cx, cy+2, mw, max(4, mh), dark);
            if (mh > 6) {
                spi->fillEllipse(cx, cy-2, mw, max(3, mh/3), teeth);
                if (mh > 12) spi->fillEllipse(cx, cy+mh/2, mw-4, mh/3, tongue);
            }
            for (float t=0.1f; t<0.9f; t+=0.06f) {
                int x = cx + (int)((t-0.5f)*2*mw);
                int y = cy + (int)((-(t-0.5f)*(t-0.5f)+0.25f)*12);
                spi->fillCircle(x, y, 2, lip);
            }
            return;
        }

        // ── Resting expression mouth ──────────────────────────────────────
        switch (exp) {
            case Expression::Happy: {
                spi->fillEllipse(cx, cy+2, 28, 16, dark);
                spi->fillEllipse(cx, cy-4, 28, 10, teeth);
                spi->fillCircle(cx, cy+8, 10, tongue);
                for (float t=0; t<1.0f; t+=0.05f) {
                    int x = cx + (int)((t-0.5f)*2*28);
                    int y = cy + (int)((-(t-0.5f)*(t-0.5f)+0.25f)*20);
                    spi->fillCircle(x, y, 2, lip);
                }
                break;
            }
            case Expression::Sad: {
                for (float t=0; t<1.0f; t+=0.05f) {
                    int x = cx + (int)((t-0.5f)*2*22);
                    int y = cy + (int)(((t-0.5f)*(t-0.5f)-0.25f)*14) + 8;
                    spi->fillCircle(x, y, 3, lip);
                }
                break;
            }
            case Expression::Doubt: {
                spi->fillRoundRect(cx-18, cy, 36, 6, 3, dark);
                spi->fillRoundRect(cx+8,  cy-3, 10, 6, 3, dark);
                break;
            }
            case Expression::Sleepy: {
                spi->fillEllipse(cx, cy+2, 12, 7, dark);
                break;
            }
            default: {
                for (float t=0.2f; t<0.8f; t+=0.05f) {
                    int x = cx + (int)((t-0.5f)*2*20);
                    int y = cy + (int)((-(t-0.5f)*(t-0.5f)+0.25f)*10);
                    spi->fillCircle(x, y, 2, lip);
                }
                break;
            }
        }
    }
};

// ══════════════════════════════════════════════════════════════
//  FacePlate — rounded rect border + hair (drawn on top)
// ══════════════════════════════════════════════════════════════
class FacePlate : public Drawable {
    uint16_t _border;
    uint16_t _bg;
    bool     _isRumi;
public:
    FacePlate(uint16_t borderCol, uint16_t bgCol, bool isRumi)
        : _border(borderCol), _bg(bgCol), _isRumi(isRumi) {}

    void draw(M5Canvas *spi, BoundingRect rect, DrawContext *ctx) override {
        // NOTE: background is set via ColorPalette — do NOT fillScreen here
        // Face plate border — height stops at 185 so speech bubble sits below
        int px=20, py=12, pw=280, ph=175, pr=28;
        spi->drawRoundRect(px+2, py+2, pw-4, ph-4, pr-2, _border);
        spi->drawRoundRect(px+3, py+3, pw-6, ph-6, pr-3, _border);

        if (!_isRumi) {
            // ── Kira — vivid purple spiky hair ──────────────
            uint16_t hair  = spi->color565(110,  65, 220);
            uint16_t hair2 = spi->color565(170, 120, 255);
            uint16_t hairS = spi->color565( 70,  40, 150);
            spi->fillEllipse(160, py-2, 105, 38, hairS);
            spi->fillEllipse(160, py-2, 100, 34, hair);
            spi->fillEllipse(135, py-2,  26, 28, hair2);
            spi->fillEllipse(135, py-2,  18, 22, hair);
            spi->fillTriangle(130, py+4, 143, py-18, 156, py+4, hair);
            spi->fillTriangle(150, py+4, 162, py-22, 174, py+4, hair);
            spi->fillTriangle(168, py+4, 178, py-12, 188, py+4, hair);
            spi->fillEllipse(160, py-2, 100, 30, hair);
            // Ahoge with cyan tip
            spi->fillRoundRect(152, py-30, 10, 28, 5, hair);
            spi->fillCircle(157, py-32, 8, spi->color565(0, 210, 255));
            spi->fillCircle(157, py-32, 4, spi->color565(180, 255, 255));
            // Gaming clip
            spi->fillRoundRect(232, py+14, 22, 12, 5, spi->color565(220, 90,  45));
            spi->fillRoundRect(234, py+16, 18,  8, 3, spi->color565(255, 155,  60));
            spi->fillCircle(234, py+20, 4, spi->color565(255, 185,  80));
        } else {
            // ── Rumi — KPop Demon Hunters style ─────────────────────────
            // Vivid purple hair, magenta demon glow, K-pop idol aesthetic
            uint16_t hair    = spi->color565(120,  30, 200);  // vivid purple
            uint16_t hair2   = spi->color565(160,  80, 240);  // lighter purple highlight
            uint16_t hairDark= spi->color565( 70,  15, 130);  // shadow
            uint16_t magenta = spi->color565(255,   0, 160);  // demon glow magenta
            uint16_t pink2   = spi->color565(255, 120, 200);  // soft pink accent

            // Hair dome — big bold purple
            spi->fillEllipse(160, py-2, 108, 42, hairDark);
            spi->fillEllipse(160, py-2, 104, 38, hair);

            // Braid coming down left side (thick purple rope)
            spi->fillRoundRect(68, py+2, 18, 55, 9, hair);
            spi->fillRoundRect(70, py+4, 14, 50, 7, hair2);
            // Braid segments
            for(int seg=0; seg<4; seg++){
                spi->drawRoundRect(69, py+8+seg*12, 16, 10, 4, hairDark);
            }
            // Braid tip
            spi->fillTriangle(68, py+57, 86, py+57, 77, py+70, hair);

            // Hair highlight streak
            spi->fillEllipse(135, py-2, 22, 30, hair2);
            spi->fillEllipse(135, py-2, 14, 24, spi->color565(180, 120, 255));

            // Clean hair base
            spi->fillEllipse(160, py-2, 104, 32, hair);

            // Demon glow star/mark on forehead (her demon heritage symbol)
            spi->fillCircle(200, py+8, 7, magenta);
            spi->fillCircle(200, py+8, 4, spi->color565(255, 160, 220));
            // Star points
            spi->fillTriangle(200, py+1,  197, py+8, 203, py+8, magenta);
            spi->fillTriangle(200, py+15, 197, py+8, 203, py+8, magenta);

            // K-pop star hair clip
            spi->fillCircle(240, py+16, 9, magenta);
            spi->fillCircle(240, py+16, 6, pink2);
            spi->fillCircle(240, py+16, 3, spi->color565(255,220,240));
        }
    }
};

// ══════════════════════════════════════════════════════════════
//  CharDetails — blush + nose drawn over the face
// ══════════════════════════════════════════════════════════════
class CharDetails : public Drawable {
    bool _isRumi;
public:
    CharDetails(bool isRumi) : _isRumi(isRumi) {}

    void draw(M5Canvas *spi, BoundingRect rect, DrawContext *ctx) override {
        Expression exp = ctx->getExpression();
        bool excited = (exp == Expression::Happy);

        uint16_t blush = _isRumi
            ? spi->color565(255, 155, 185)
            : spi->color565(255, 160, 145);
        uint16_t blushH = spi->color565(255, 205, 220);

        int ba = excited ? 255 : 130;
        (void)ba;

        spi->fillEllipse( 62, 150, excited?26:22, excited?12:9, blush);
        spi->fillEllipse( 62, 150, excited?16:13, excited? 7:5, blushH);
        spi->fillEllipse(258, 152, excited?26:22, excited?12:9, blush);
        spi->fillEllipse(258, 152, excited?16:13, excited? 7:5, blushH);

        spi->fillCircle(154, 160, 3, spi->color565(200, 160, 145));
        spi->fillCircle(164, 160, 3, spi->color565(200, 160, 145));
    }
};
