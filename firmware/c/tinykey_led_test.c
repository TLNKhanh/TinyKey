// ============================================================================
//  TinyKey — LED matrix text + effects firmware (CH32V003F4P6)
//
//  Board: TinyKey (see hardware/ in this repo)
//
//  7x7 multiplexed LED matrix D1..D49 on 14 dedicated GPIO lines:
//    anode lines A1..A7 -> PC0..PC6
//    cathode lines K1..K7 -> PC7, PD0, PD2, PD3, PD4, PD5, PD6
//  D_n -> row=(n-1)/7 (anode), col=(n-1)%7 (cathode)
//
//  All 49 front LEDs are soldered in BACKWARDS, so the drive polarity is
//  swapped: anode line LOW + cathode line HIGH (see POL_REVERSED).
//
//  Driving: ROW SCAN. One anode line is driven per step and only the lit
//  cathode lines for that row are enabled; everything else stays high-Z.
//  7 steps/frame max -> duty ~1/7, high refresh -> no visible flicker.
//
//  NOTE: no series current-limit resistors on this matrix. Peak current is
//  bounded by the slow 2 MHz GPIO drive setting. Do not raise it.
//
//  Controls:
//    BTN1 (PD7)  -> next effect (cycles through all of them)
//    BTN2 (PA1)  -> sleep / wake
//
//  IMPORTANT: PD7 is also NRST on the CH32V003. The reset function must be
//  disabled via the option bytes (OB->USER bits 3-4 = 11) or pressing BTN1
//  resets the chip instead of switching effect. See README.md.
// ============================================================================

#include "ch32v003fun.h"

// 1 = front LEDs are soldered backwards (invert drive polarity)
#ifndef POL_REVERSED
#define POL_REVERSED 1
#endif

// per-row dwell in microseconds. The dwell is held for EVERY row, lit or not,
// so a frame is always 7 * DWELL_US long and the duty cycle never follows the
// picture (skipping empty rows makes the panel pump at the scroll rate).
#define DWELL_US 250

// base animation step interval (ms) — everything except the text scroll
#define STEP_MS 50
#define STEP_COUNTS ((uint32_t)((FUNCONF_SYSTEM_CORE_CLOCK / 1000UL) * STEP_MS))

// text scroll step interval (ms). The text always advances exactly ONE column
// per step; this is how fast it goes. Lower = faster.
#define SCROLL_MS 20
#define SCROLL_COUNTS ((uint32_t)((FUNCONF_SYSTEM_CORE_CLOCK / 1000UL) * SCROLL_MS))

// displayed message
#define MSG "Nam Khanh"

// ---------------------------------------------------------------------------
//  Pin map
// ---------------------------------------------------------------------------
static const uint8_t ANODE_PORT[7] = {0,0,0,0,0,0,0};   // 0 = port C
static const uint8_t ANODE_PIN [7] = {0,1,2,3,4,5,6};
static const uint8_t CATH_PORT[7]  = {0,1,1,1,1,1,1};   // 0 = port C, 1 = port D
static const uint8_t CATH_PIN [7]  = {7,0,2,3,4,5,6};

#define BTN1_PIN 7   // port D
#define BTN2_PIN 1   // port A

#define ST_HZ 0
#define ST_HI 1
#define ST_LO 2

// ---------------------------------------------------------------------------
//  GPIO helpers
// ---------------------------------------------------------------------------
static inline volatile uint32_t *cfg_reg(uint8_t isD, uint8_t isA){
    if(isD) return &GPIOD->CFGLR;
    if(isA) return &GPIOA->CFGLR;
    return &GPIOC->CFGLR;
}
static inline volatile uint32_t *bshr_reg(uint8_t isD, uint8_t isA){
    if(isD) return &GPIOD->BSHR;
    if(isA) return &GPIOA->BSHR;
    return &GPIOC->BSHR;
}

// set one pin: hi / lo / high-impedance (glitch-free: level first, then mode)
static void drive(uint8_t isD, uint8_t isA, uint8_t pin, uint8_t st){
    volatile uint32_t *cfg  = cfg_reg(isD, isA);
    volatile uint32_t *bshr = bshr_reg(isD, isA);
    if(st == ST_HZ){
        *cfg = (*cfg & ~(0xfUL << (4*pin))) | ((uint32_t)GPIO_CNF_IN_FLOATING << (4*pin));
    } else {
        if(st == ST_HI) *bshr = (1UL << pin);
        else            *bshr = (1UL << (16+pin));
        *cfg = (*cfg & ~(0xfUL << (4*pin))) | ((uint32_t)(GPIO_Speed_2MHz | GPIO_CNF_OUT_PP) << (4*pin));
    }
}

static void all_lines_hz(void){
    uint8_t i;
    for(i=0;i<7;i++){
        drive(ANODE_PORT[i], 0, ANODE_PIN[i], ST_HZ);
        drive(CATH_PORT[i],  0, CATH_PIN[i],  ST_HZ);
    }
}

// ---------------------------------------------------------------------------
//  Buttons (active low, internal pull-up)
// ---------------------------------------------------------------------------
static void btn_setup(uint8_t isA, uint8_t pin){
    volatile uint32_t *cfg = cfg_reg(0, isA);
    volatile uint32_t *odr = isA ? &GPIOA->OUTDR : &GPIOD->OUTDR;
    *odr |= (1UL << pin);
    *cfg = (*cfg & ~(0xfUL << (4*pin))) | ((uint32_t)GPIO_CNF_IN_PUPD << (4*pin));
}
static uint8_t btn_read(uint8_t isA, uint8_t pin){
    volatile const uint32_t *idr = isA ? &GPIOA->INDR : &GPIOD->INDR;
    return (((*idr >> pin) & 1) == 0);
}

// ---------------------------------------------------------------------------
//  5x7 font. Each glyph = 5 column bytes, bit0 = top row, bit6 = bottom row.
// ---------------------------------------------------------------------------
static const char FONT_CHARS[] =
    " !'-.,:?0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static const uint8_t FONT[] = {
    0x00,0x00,0x00,0x00,0x00,  // ' '
    0x00,0x00,0x5F,0x00,0x00,  // '!'
    0x00,0x07,0x00,0x00,0x00,  // '''
    0x08,0x08,0x08,0x08,0x08,  // '-'
    0x00,0x60,0x60,0x00,0x00,  // '.'
    0x00,0x40,0x20,0x00,0x00,  // ','
    0x00,0x36,0x36,0x00,0x00,  // ':'
    0x02,0x01,0x51,0x09,0x06,  // '?'
    0x3E,0x51,0x49,0x45,0x3E,  // 0
    0x00,0x42,0x7F,0x40,0x00,  // 1
    0x42,0x61,0x51,0x49,0x46,  // 2
    0x21,0x41,0x45,0x4B,0x31,  // 3
    0x18,0x14,0x12,0x7F,0x10,  // 4
    0x27,0x45,0x45,0x45,0x39,  // 5
    0x3C,0x4A,0x49,0x49,0x30,  // 6
    0x01,0x71,0x09,0x05,0x03,  // 7
    0x36,0x49,0x49,0x49,0x36,  // 8
    0x06,0x49,0x49,0x29,0x1E,  // 9
    0x7E,0x09,0x09,0x09,0x7E,  // A
    0x7F,0x49,0x49,0x49,0x36,  // B
    0x3E,0x41,0x41,0x41,0x22,  // C
    0x7F,0x41,0x41,0x22,0x1C,  // D
    0x7F,0x49,0x49,0x49,0x41,  // E
    0x7F,0x09,0x09,0x09,0x01,  // F
    0x3E,0x41,0x41,0x51,0x73,  // G
    0x7F,0x08,0x08,0x08,0x7F,  // H
    0x00,0x41,0x7F,0x41,0x00,  // I
    0x20,0x40,0x41,0x3F,0x01,  // J
    0x7F,0x08,0x14,0x22,0x41,  // K
    0x7F,0x40,0x40,0x40,0x40,  // L
    0x7F,0x02,0x1C,0x02,0x7F,  // M
    0x7F,0x04,0x08,0x10,0x7F,  // N
    0x3E,0x41,0x41,0x41,0x3E,  // O
    0x7F,0x09,0x09,0x09,0x06,  // P
    0x3E,0x41,0x51,0x21,0x5E,  // Q
    0x7F,0x09,0x19,0x29,0x46,  // R
    0x46,0x49,0x49,0x49,0x31,  // S
    0x01,0x01,0x7F,0x01,0x01,  // T
    0x3F,0x40,0x40,0x40,0x3F,  // U
    0x1F,0x20,0x40,0x20,0x1F,  // V
    0x3F,0x40,0x38,0x40,0x3F,  // W
    0x63,0x14,0x08,0x14,0x63,  // X
    0x07,0x08,0x70,0x08,0x07,  // Y
    0x61,0x51,0x49,0x45,0x43,  // Z
    0x20,0x54,0x54,0x54,0x78,  // a
    0x7F,0x48,0x44,0x44,0x38,  // b
    0x38,0x44,0x44,0x44,0x20,  // c
    0x38,0x44,0x44,0x48,0x7F,  // d
    0x38,0x54,0x54,0x54,0x18,  // e
    0x08,0x7E,0x09,0x01,0x02,  // f
    0x08,0x14,0x54,0x54,0x3C,  // g
    0x7F,0x08,0x04,0x04,0x78,  // h
    0x00,0x44,0x7D,0x40,0x00,  // i
    0x20,0x40,0x44,0x3D,0x00,  // j
    0x7F,0x10,0x28,0x44,0x00,  // k
    0x00,0x41,0x7F,0x40,0x00,  // l
    0x7C,0x04,0x18,0x04,0x78,  // m
    0x7C,0x08,0x04,0x04,0x78,  // n
    0x38,0x44,0x44,0x44,0x38,  // o
    0x7C,0x14,0x14,0x14,0x08,  // p
    0x08,0x14,0x14,0x18,0x7C,  // q
    0x7C,0x08,0x04,0x04,0x08,  // r
    0x48,0x54,0x54,0x54,0x20,  // s
    0x04,0x3F,0x44,0x40,0x20,  // t
    0x3C,0x40,0x40,0x20,0x7C,  // u
    0x1C,0x20,0x40,0x20,0x1C,  // v
    0x3C,0x40,0x30,0x40,0x3C,  // w
    0x44,0x28,0x10,0x28,0x44,  // x
    0x0C,0x50,0x50,0x50,0x3C,  // y
    0x44,0x64,0x54,0x4C,0x44,  // z
};

static int font_index(char ch){
    int i;
    for(i=0; FONT_CHARS[i]; i++) if(FONT_CHARS[i]==ch) return i;
    return 7;  // '?'
}

// ---------------------------------------------------------------------------
//  Framebuffer (bit c of fb[r] = pixel at row r, column c)
// ---------------------------------------------------------------------------
static uint8_t fb[7];

// scroll position — runs on its own faster tick (SCROLL_MS), one column per step
static uint32_t g_sp = 0;

#define MSG_N     ((int)(sizeof(MSG)-1))   // characters
#define MSG_COLS  (MSG_N * 6)              // 5 columns + 1 gap each

static void fb_clear(void){
    uint8_t r; for(r=0;r<7;r++) fb[r]=0;
}
static inline void fb_set(uint8_t r, uint8_t c){ fb[r] |= (uint8_t)(1u<<c); }

// column `i` of a string (n chars), 0 outside it
static uint8_t str_col(const char *s, int i){
    if(i < 0 || i >= MSG_COLS) return 0;
    int ch = i/6, k = i%6;
    if(k==5) return 0;                       // inter-character gap
    return FONT[font_index(s[ch])*5 + k];
}

// small deterministic hash (for sparkle / fire)
static uint32_t hash32(uint32_t x){
    x ^= x>>13; x *= 1274126177u; x ^= x>>16;
    return x;
}

// ---------------------------------------------------------------------------
//  Effects
// ---------------------------------------------------------------------------
#define FX_SCROLL   0
#define FX_WAVE     1
#define FX_SINE     2
#define FX_RIPPLE   3
#define FX_FIRE     4
#define FX_RAIN     5
#define FX_SPARKLE  6
#define FX_HEART    7
#define FX_BALL     8
#define FX_CHEVRON  9
#define FX_SPINNER  10
#define FX_CHECKER  11
#define FX_BOX      12
#define FX_BLINK    13
#define FX_COUNT    14

static const int8_t WAVE[8] = {0,1,2,2,1,0,-1,-2};

// '>' chevron, 5 columns, repeated every 6 with a gap
static const uint8_t CHEV[5] = {0x41,0x22,0x14,0x08,0x08};

static void build_frame(uint8_t fx, uint32_t p){
    uint8_t r, c;

    fb_clear();

    switch(fx){

    case FX_SCROLL: {                       // message scrolling right-to-left
        int base = (int)(g_sp % (uint32_t)(MSG_COLS + 7));
        for(c=0;c<7;c++){
            uint8_t col = str_col(MSG, base + (int)c - 6);
            for(r=0;r<7;r++)
                if(col & (1u<<r)) fb_set(r,c);
        }
        break;
    }

    case FX_WAVE: {                         // same text, waving vertically
        int base = (int)(g_sp % (uint32_t)(MSG_COLS + 7));
        for(c=0;c<7;c++){
            uint8_t col = str_col(MSG, base + (int)c - 6);
            int8_t off = WAVE[(base + c) & 7];
            for(r=0;r<7;r++){
                int sr = (int)r - off;
                if(sr>=0 && sr<7 && (col & (1u<<sr))) fb_set(r,c);
            }
        }
        break;
    }

    case FX_SINE: {                         // classic travelling wave
        for(c=0;c<7;c++){
            int row = 3 + WAVE[(c + (int)p) & 7];
            if(row>=0 && row<7) fb_set((uint8_t)row,c);
        }
        break;
    }

    case FX_RIPPLE: {                       // expanding rings from the centre
        int phase = (int)(p % 5);
        for(r=0;r<7;r++){
            for(c=0;c<7;c++){
                int dr = (r>3) ? (r-3) : (3-r);
                int dc = (c>3) ? (c-3) : (3-c);
                int d  = (dr>dc) ? dr : dc;     // chebyshev distance
                if(d == phase || d == (phase-1)) fb_set(r,c);
            }
        }
        break;
    }

    case FX_FIRE: {                         // flame: per-column height, flickering
        static uint8_t heat[7] = {1,3,5,7,5,3,1};
        static uint32_t lastp = 0xFFFFFFFFu;
        if(p != lastp){
            lastp = p;
            for(c=0;c<7;c++){
                int8_t d = (int8_t)(hash32(p*2654435761u + c*40503u) % 3u) - 1;
                int16_t v = (int16_t)heat[c] + d;
                if(v < 1) v = 1;
                if(v > 7) v = 7;
                heat[c] = (uint8_t)v;
            }
        }
        for(c=0;c<7;c++)
            for(r=0;r<7;r++)
                if((7-r) <= heat[c]) fb_set(r,c);
        break;
    }

    case FX_RAIN: {                         // falling drops with a short trail
        for(c=0;c<7;c++){
            int drop = (int)((p/2 + (uint32_t)(c*5)) % 12) - 4;
            int k;
            for(k=0;k<3;k++){
                int rr = drop - k;
                if(rr>=0 && rr<7) fb_set((uint8_t)rr, c);
            }
        }
        break;
    }

    case FX_SPARKLE: {                      // random twinkle
        for(r=0;r<7;r++)
            for(c=0;c<7;c++)
                if(hash32(p*2654435761u + (uint32_t)(r*7+c)*2246822519u) & 1u)
                    fb_set(r,c);
        break;
    }

    case FX_HEART: {                        // beating heart
        static const uint8_t BIG[7] = {0x36,0x7F,0x7F,0x3E,0x1C,0x08,0x00};
        static const uint8_t SM [7] = {0x00,0x36,0x3E,0x1C,0x08,0x00,0x00};
        const uint8_t *h = ((p/6) & 1) ? BIG : SM;
        for(r=0;r<7;r++) fb[r] = h[r];
        break;
    }

    case FX_BALL: {                         // bouncing ball with a trail
        static int8_t bx = 0, by = 0, vx = 1, vy = 1;
        static uint32_t lastp = 0xFFFFFFFFu;
        if(p != lastp){
            lastp = p;
            if((p & 1u) == 0){
                bx += vx; by += vy;
                if(bx < 0){ bx = 0; vx = 1; }
                if(bx > 6){ bx = 6; vx = -1; }
                if(by < 0){ by = 0; vy = 1; }
                if(by > 6){ by = 6; vy = -1; }
            }
        }
        fb_set((uint8_t)by, (uint8_t)bx);
        break;
    }

    case FX_CHEVRON: {                      // '>' arrows marching right
        for(c=0;c<7;c++){
            int x = (int)((p + c) % 6);
            uint8_t col = (x < 5) ? CHEV[x] : 0;
            for(r=0;r<7;r++)
                if(col & (1u<<r)) fb_set(r,c);
        }
        break;
    }

    case FX_SPINNER: {                      // rotating radius
        static const int8_t DR[8] = { 0, 1, 1, 1, 0,-1,-1,-1};
        static const int8_t DC[8] = { 1, 1, 0,-1,-1,-1, 0, 1};
        int a = (int)(p % 8);
        int rr = 3, cc = 3, k;
        for(k=0;k<3;k++){
            rr += DR[a]; cc += DC[a];
            if(rr<0||rr>6||cc<0||cc>6) break;
            fb_set((uint8_t)rr,(uint8_t)cc);
        }
        fb_set(3,3);
        break;
    }

    case FX_CHECKER:                        // animated checkerboard
        for(r=0;r<7;r++)
            for(c=0;c<7;c++)
                if(((r + c + (uint8_t)(p/2)) & 1)==0) fb_set(r,c);
        break;

    case FX_BOX: {                          // expanding / contracting square
        int s = (int)(p % 9);
        int half = s/2;
        int lo = 3-half, hi = 3+half;
        if(lo<0) lo=0;
        if(hi>6) hi=6;
        for(c=0;c<7;c++){
            if(c<lo || c>hi) continue;
            fb_set(lo,c); fb_set(hi,c);
        }
        for(r=0;r<7;r++){
            if(r<lo || r>hi) continue;
            fb_set(r,lo); fb_set(r,hi);
        }
        break;
    }

    case FX_BLINK:                          // whole display blink
        if(((p/6) & 1)==0)
            for(r=0;r<7;r++) fb[r] = 0x7F;
        break;
    }
}

// ---------------------------------------------------------------------------
//  Renderer: row scan (one anode line per step, only lit cathodes enabled)
// ---------------------------------------------------------------------------
static void render_grid(void){
    uint8_t r, c;
    const uint8_t a_st = POL_REVERSED ? ST_LO : ST_HI;
    const uint8_t k_st = POL_REVERSED ? ST_HI : ST_LO;

    for(r=0;r<7;r++){
        uint8_t mask = (uint8_t)(fb[r] & 0x7F);

        drive(ANODE_PORT[r], 0, ANODE_PIN[r], a_st);
        for(c=0;c<7;c++)
            if(mask & (1u<<c)) drive(CATH_PORT[c], 0, CATH_PIN[c], k_st);

        Delay_Us(DWELL_US);

        drive(ANODE_PORT[r], 0, ANODE_PIN[r], ST_HZ);
        for(c=0;c<7;c++)
            if(mask & (1u<<c)) drive(CATH_PORT[c], 0, CATH_PIN[c], ST_HZ);
    }
}

// ---------------------------------------------------------------------------
int main(void){
    SystemInit();

    RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD;

    all_lines_hz();
    btn_setup(0, BTN1_PIN);   // PD7
    btn_setup(1, BTN2_PIN);   // PA1

    uint8_t  fx     = 0;
    uint32_t p      = 0;
    uint32_t last   = SysTick->CNT;
    uint32_t slast  = SysTick->CNT;
    uint8_t  prev1  = 1, prev2 = 1;
    uint8_t  asleep = 0;

    while(1){
        if(!asleep){
            build_frame(fx, p);
            render_grid();

            if((uint32_t)(SysTick->CNT - last) >= STEP_COUNTS){
                last = SysTick->CNT;
                p++;
            }
            if((uint32_t)(SysTick->CNT - slast) >= SCROLL_COUNTS){
                slast = SysTick->CNT;
                g_sp++;
            }
        } else {
            // sleep: display fully off (all lines high-Z), poll for wake
            all_lines_hz();
            Delay_Ms(20);
        }

        uint8_t b1 = btn_read(0, BTN1_PIN);
        uint8_t b2 = btn_read(1, BTN2_PIN);

        if(b1 && !prev1 && !asleep){          // BTN1: next effect
            fx = (uint8_t)((fx + 1) % FX_COUNT);
            p  = 0;
            g_sp = 0;
            last = slast = SysTick->CNT;
        }
        if(b2 && !prev2){                     // BTN2: sleep / wake
            asleep = !asleep;
            if(!asleep){ p = 0; g_sp = 0; last = slast = SysTick->CNT; }
        }
        prev1 = b1; prev2 = b2;
    }
}
