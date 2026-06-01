#pragma once
/*
  kira_faces.h — Clean character faces
  Skin background (from palette) = face
  Hair dome overlay covers top = instant "hair" look
  Avatar eyebrows + our 3D eyes + blush = proper character

  Face geometry (320×240):
    Hair dome stops at y≈55
    Eyebrows:  y≈67-72   (12px below hair — visible forehead)
    Eyes:      y≈93-96   (well below hair)
    Blush:     y≈118-125
    Mouth:     y≈148
*/
#include <Avatar.h>
using namespace m5avatar;

// ════════════════════════════════════════════════════════════
//  3D Anime Eye — iris, pupil, shine, eyelid shadow
// ════════════════════════════════════════════════════════════
class AnimeEye : public Eye {
    bool     _isLeft;
    uint16_t _iris;
public:
    AnimeEye(bool isLeft, uint16_t irisColor)
        : Eye(8, isLeft), _isLeft(isLeft), _iris(irisColor) {}

    void draw(M5Canvas* spi, BoundingRect rect, DrawContext* ctx) override {
        Expression exp = ctx->getExpression();
        int cx = rect.getCenterX(), cy = rect.getCenterY();
        Gaze g   = _isLeft ? ctx->getLeftGaze() : ctx->getRightGaze();
        float op = _isLeft ? ctx->getLeftEyeOpenRatio() : ctx->getRightEyeOpenRatio();
        int ox = (int)(g.getHorizontal()*4), oy = (int)(g.getVertical()*3);

        uint16_t bg    = ctx->getColorPalette()->get(COLOR_BACKGROUND);
        uint16_t white = spi->color565(255, 255, 255);
        uint16_t dark  = spi->color565(10,  6,   20);
        uint16_t shine = spi->color565(210, 220, 255);

        // Lighter iris ring for depth
        int rv=_iris>>11, gv=(_iris>>5)&0x3F, bv=_iris&0x1F;
        uint16_t irisLight = spi->color565(
            min(255,rv*8+80), min(255,gv*4+55), min(255,bv*8+80));

        const int RW=13, RH_MAX=12;
        int rh = (int)(RH_MAX*op);

        if (op > 0.06f) {
            // Sclera
            spi->fillEllipse(cx+ox, cy+oy, RW, rh, white);
            // Iris
            spi->fillCircle(cx+ox,    cy+oy,    (int)(RW*0.76f), _iris);
            // Inner lighter ring
            spi->fillCircle(cx+ox-1,  cy+oy-2,  (int)(RW*0.44f), irisLight);
            // Pupil
            spi->fillCircle(cx+ox,    cy+oy,    (int)(RW*0.28f), dark);
            // Shine dots
            spi->fillCircle(cx+ox-4,  cy+oy-4,  3, white);
            spi->fillCircle(cx+ox+3,  cy+oy-2,  1, shine);
            // Top eyelid shadow
            spi->fillRect(cx+ox-RW, cy+oy-rh, RW*2, max(1,(int)(rh*0.22f)),
                spi->color565(130, 100, 165));

            // Expression cutoffs
            if (exp==Expression::Angry||exp==Expression::Sad) {
                int x0=cx+ox-RW, y0=cy+oy-rh, x1=x0+RW*2;
                int x2=(!_isLeft)!=(exp==Expression::Sad)?x0:x1;
                spi->fillTriangle(x0,y0, x1,y0, x2,y0+rh, bg);
            }
            if (exp==Expression::Happy||exp==Expression::Sleepy) {
                int x0=cx+ox-RW, y0=cy+oy-rh, w=RW*2+2, h2=rh+1;
                if(exp==Expression::Happy){
                    y0+=rh;
                    spi->fillCircle(cx+ox,cy+oy,(int)(RW*0.5f),bg);
                }
                spi->fillRect(x0,y0,w,h2,bg);
            }
        } else {
            spi->fillRect(cx-RW+ox, cy-2+oy, RW*2, 3, _iris);
        }
    }
};

// ════════════════════════════════════════════════════════════
//  Curved Mouth
// ════════════════════════════════════════════════════════════
class AnimeMouth : public Mouth {
public:
    AnimeMouth() : Mouth(50,90,4,60) {}
    void draw(M5Canvas* spi, BoundingRect rect, DrawContext* ctx) override {
        uint16_t dark = spi->color565(55, 18, 8);
        uint16_t lip  = spi->color565(200, 100, 75);
        float open = ctx->getMouthOpenRatio();
        float breath = ctx->getBreath();
        int cx = rect.getCenterX();
        int cy = rect.getCenterY() + (int)(breath*2);

        if (open < 0.06f) {
            // Smile arc ∪
            for (int i=-22; i<=22; i++) {
                float t=i/22.0f;
                spi->fillCircle(cx+(int)(t*28), cy+8-(int)(t*t*11), 2, lip);
            }
        } else {
            int mw=(int)(15+open*22), mh=(int)(3+open*18);
            spi->fillEllipse(cx,cy,mw,mh,dark);
            if (open>0.45f)
                spi->fillRect(cx-mw/2+2, cy-mh+2, mw-4, mh/2,
                    spi->color565(235,215,205));
            for (int i=-22; i<=22; i++) {
                float t=i/22.0f;
                spi->fillCircle(cx+(int)(t*mw), cy-mh+(int)(t*t*5), 2, lip);
            }
        }
    }
};

// ════════════════════════════════════════════════════════════
//  RGB screen border — changes with expression
// ════════════════════════════════════════════════════════════
class RGBBorder : public Drawable {
    bool _isRumi;
public:
    RGBBorder(bool isRumi) : _isRumi(isRumi) {}
    void draw(M5Canvas* spi, BoundingRect rect, DrawContext* ctx) override {
        Expression exp   = ctx->getExpression();
        float      pulse = (ctx->getBreath()+1.0f)*0.5f;
        uint16_t col, col2;
        if (_isRumi) {
            switch(exp) {
                case Expression::Happy:  col=spi->color565(255,60,170);  col2=spi->color565(255,150,215); break;
                case Expression::Sad:    col=spi->color565(170,90,210);  col2=spi->color565(200,140,240); break;
                case Expression::Sleepy: col=spi->color565(195,130,225); col2=spi->color565(215,165,235); break;
                case Expression::Doubt:  col=spi->color565(255,130,60);  col2=spi->color565(255,175,110); break;
                default:                 col=spi->color565(255,110,195); col2=spi->color565(255,170,225); break;
            }
        } else {
            switch(exp) {
                case Expression::Happy:  col=spi->color565(255,210,0);   col2=spi->color565(255,235,120); break;
                case Expression::Sad:    col=spi->color565(50,80,255);   col2=spi->color565(110,140,255); break;
                case Expression::Sleepy: col=spi->color565(110,55,220);  col2=spi->color565(155,105,245); break;
                case Expression::Doubt:  col=spi->color565(255,70,70);   col2=spi->color565(255,140,110); break;
                default:                 col=spi->color565(160,80,255);  col2=spi->color565(200,150,255); break; // purple default
            }
        }
        // Border lines
        for (int i=0;i<3;i++) spi->drawRect(i,i,320-i*2,240-i*2,col);
        for (int i=3;i<6;i++) spi->drawRect(i,i,320-i*2,240-i*2,col2);
        // Corner brackets
        int cs=22, ct=4;
        spi->fillRect(0,0,cs,ct,col);   spi->fillRect(0,0,ct,cs,col);
        spi->fillRect(320-cs,0,cs,ct,col); spi->fillRect(316,0,ct,cs,col);
        spi->fillRect(0,236,cs,ct,col); spi->fillRect(0,220,ct,cs,col);
        spi->fillRect(320-cs,236,cs,ct,col); spi->fillRect(316,220,ct,cs,col);
        // Breathing pulse dot top-centre
        int dr = 4+(int)(pulse*3);
        spi->fillCircle(160,6,dr+1,col2);
        spi->fillCircle(160,6,dr-1,col);
        spi->fillCircle(160,6,2,spi->color565(255,255,255));
    }
};

// ════════════════════════════════════════════════════════════
//  Character overlay — hair dome + blush + details
//  Hair dome covers y=0→55 — Avatar eyebrows visible at y≈67
// ════════════════════════════════════════════════════════════
class CharOverlay : public Drawable {
    bool _isRumi;
public:
    CharOverlay(bool isRumi) : _isRumi(isRumi) {}
    void draw(M5Canvas* spi, BoundingRect rect, DrawContext* ctx) override {
        uint16_t skin  = ctx->getColorPalette()->get(COLOR_BACKGROUND);

        if (_isRumi) {
            // ── Rumi — purple hair dome ───────────────────────────────────
            uint16_t hair  = spi->color565(110, 55, 175);
            uint16_t hair2 = spi->color565(145, 90, 210);

            // Clean dome — stops at y=55, well above eyebrows
            spi->fillEllipse(160, 0, 115, 58, hair);
            // Single left-side shine streak only
            spi->fillEllipse(122, 0, 22, 42, hair2);
            spi->fillEllipse(122, 0, 16, 36, hair);

            // Blush — soft pink ovals
            spi->fillEllipse(60,  120, 22, 10, spi->color565(255,185,215));
            spi->fillEllipse(260, 123, 22, 10, spi->color565(255,185,215));
            spi->fillEllipse(60,  120, 14,  6, spi->color565(255,210,230));
            spi->fillEllipse(260, 123, 14,  6, spi->color565(255,210,230));

            // Tiny pink heart mark on cheek
            spi->fillCircle(52, 116, 5, spi->color565(255,80,160));
            spi->fillCircle(58, 116, 5, spi->color565(255,80,160));
            spi->fillTriangle(47,118, 55,125, 63,118, spi->color565(255,80,160));

            // Nose dots
            spi->fillCircle(156,131,2,spi->color565(210,170,150));
            spi->fillCircle(163,131,2,spi->color565(210,170,150));

            // Hair sparkle
            spi->fillCircle(160, 12, 4, spi->color565(255,255,200));
            spi->fillCircle(159, 11, 2, spi->color565(255,255,255));

        } else {
            // ── Kira — bright purple gamer hair ─────────────────────────
            uint16_t hair  = spi->color565(95,  55, 210);   // vivid purple — visible on dark bg
            uint16_t hair2 = spi->color565(170, 110, 255);  // bright highlight streak
            uint16_t hair3 = spi->color565(55,  35, 140);   // shadow/depth

            // Main dome
            spi->fillEllipse(160, 0, 115, 58, hair);
            // Shadow sides for depth
            spi->fillEllipse(55,  20, 28, 48, hair3);
            spi->fillEllipse(265, 20, 28, 48, hair3);
            // Bright shine streak down centre-left
            spi->fillEllipse(128, 0, 24, 46, hair2);
            spi->fillEllipse(128, 0, 16, 38, hair);

            // Spiky fringe bits
            spi->fillTriangle(125,-1, 138,-20, 151,-1, hair);
            spi->fillTriangle(148,-1, 160,-22, 172,-1, hair);
            spi->fillTriangle(170,-1, 180,-14, 190,-1, hair);
            // Clean base
            spi->fillEllipse(160, 0, 115, 55, hair);

            // Ahoge (little strand) with cyan tip — very gamery
            spi->fillEllipse(150, 0, 5, 24, hair);
            spi->fillCircle(148,-22, 6, spi->color565(0, 220, 255));   // cyan tip
            spi->fillCircle(148,-22, 3, spi->color565(180, 255, 255)); // bright centre

            // Blush — subtle warm tones
            spi->fillEllipse(60,  120, 20,  9, spi->color565(240,175,160));
            spi->fillEllipse(260, 123, 20,  9, spi->color565(240,175,160));

            // Nose dots
            spi->fillCircle(156,131,2,spi->color565(210,170,150));
            spi->fillCircle(163,131,2,spi->color565(210,170,150));

            // Gaming headset clip — bright orange
            spi->fillRoundRect(206,36,22,12,5,spi->color565(220,90,45));
            spi->fillRoundRect(208,38,18, 8,3,spi->color565(255,150,60));
            spi->fillCircle(206,42, 4, spi->color565(255,180,80));  // glowy dot
        }
    }
};
