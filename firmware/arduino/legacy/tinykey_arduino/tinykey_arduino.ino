// ============================================================================
//  TinyKey -- LED matrix text + effects        (CH32V003F4P6, Arduino / WCH)
//
//  Board:  TinyKey
//  FQBN:   WCH:ch32v:CH32V00x_EVT:pnum=CH32V003F4
//          Arduino IDE: Tools > Board > CH32 MCU EVT Boards > CH32V00x
//
//  7x7 multiplexed LED matrix D1..D49 on 14 GPIO lines:
//    anode   A1..A7 -> PC0..PC6
//    cathode K1..K7 -> PC7, PD0, PD2, PD3, PD4, PD5, PD6
//  D_n -> row = (n-1)/7 (anode line), col = (n-1)%7 (cathode line)
//
//  All 49 front LEDs are soldered in BACKWARDS, so the drive polarity is
//  inverted -> POL_REVERSED 1. That is the only place it matters.
//
//  Driving: ROW SCAN. One anode line is driven per step and only that row's
//  lit cathodes are enabled; everything else stays high-Z. ~1/7 duty,
//  ~570 Hz refresh -> no visible flicker.
//
//  !! There are NO series current-limit resistors on this matrix. The peak
//     current is bounded by the slow 2 MHz GPIO drive setting -- do NOT raise
//     it to 10/50 MHz. (The Arduino pinMode(OUTPUT) helper is not used for the
//     matrix for exactly this reason; the 2 MHz nibble is written directly.)
//
//  Buttons (switches to GND, internal pull-up, active low):
//    PD7 -> next effect
//    PA1 -> sleep / wake
//
//  PD7 is also NRST. The reset function MUST be disabled in the option bytes
//  (USER bits 3-4 = 11), otherwise pressing this button resets the chip
//  instead of switching effect. Already done on this board -- see README.md.
// ============================================================================

#include <Arduino.h>

// 1 = front LEDs soldered backwards (invert drive polarity)
#define POL_REVERSED 1

// per-row dwell in microseconds (frame time = lit_rows * DWELL_US)
#define DWELL_US 250

// base animation step interval (ms) -- everything except the text scroll
#define STEP_MS 50

// text scroll step interval (ms). The text always advances exactly ONE column
// per step; this is how fast it goes. Lower = faster. (No pixel skipping.)
#define SCROLL_MS 20

// displayed message
#define MSG "Nam Khanh"

// ---------------------------------------------------------------------------
//  Pin map (Arduino pin names from variant_CH32V003F4.h)
// ---------------------------------------------------------------------------
static const uint8_t ANODE_PIN[7] = { PC0, PC1, PC2, PC3, PC4, PC5, PC6 };
static const uint8_t CATH_PIN [7] = { PC7, PD0, PD2, PD3, PD4, PD5, PD6 };

#define BTN_FX  PD7   // next effect
#define BTN_SLP PA1   // sleep / wake

#define ST_HZ 0
#define ST_HI 1
#define ST_LO 2

// ---------------------------------------------------------------------------
//  Raw GPIO helpers (port 0 = C, 1 = D, 2 = A)
// ---------------------------------------------------------------------------
static inline volatile uint32_t *cfg_reg(uint8_t p){
    if(p == 1) return &GPIOD->CFGLR;
    if(p == 2) return &GPIOA->CFGLR;
    return &GPIOC->CFGLR;
}
static inline volatile uint32_t *bshr_reg(uint8_t p){
    if(p == 1) return &GPIOD->BSHR;
    if(p == 2) return &GPIOA->BSHR;
    return &GPIOC->BSHR;
}

// CFGLR nibble values, encoded exactly the way the core's GPIO_Init() does it:
//   input  -> nibble = Mode & 0x0F
//   output -> nibble = (Mode & 0x0F) | Speed
#define CFG_HZ        ((uint32_t)GPIO_Mode_IN_FLOATING)                          // 0x04
#define CFG_OUT_2MHZ  ((uint32_t)((GPIO_Mode_Out_PP & 0x0F) | GPIO_Speed_2MHz))   // 0x02

// set one pin: hi / lo / high-impedance. Glitch-free: level first, then mode.
static void drive(uint8_t prt, uint8_t pin, uint8_t st){
    volatile uint32_t *cfg  = cfg_reg(prt);
    volatile uint32_t *bshr = bshr_reg(prt);
    if(st == ST_HZ){
        *cfg = (*cfg & ~(0xfUL << (4*pin))) | (CFG_HZ << (4*pin));
    } else {
        if(st == ST_HI) *bshr = (1UL << pin);
        else            *bshr = (1UL << (16+pin));
        *cfg = (*cfg & ~(0xfUL << (4*pin))) | (CFG_OUT_2MHZ << (4*pin));
    }
}

static void all_lines_hz(void){
    uint8_t i;
    for(i=0;i<7;i++){
        drive(0, ANODE_PIN[i], ST_HZ);
        drive(0, CATH_PIN[i],  ST_HZ);
    }
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

// Note: this deliberately scans the WHOLE table instead of returning on the
// first hit. A frame is 7 x DWELL_US of dwell plus the frame-build time, and
// brightness is dwell/frame_time -- so a lookup whose cost depended on which
// character it is (a space matches on the first compare, an 'h' needs 60+)
// would make the brightness ripple as different characters scrolled past.
// Constant cost keeps the frame time (and therefore the duty) constant.
static int font_index(char ch){
    int i, found = 7;                       // '?' is the fallback
    for(i=0; FONT_CHARS[i]; i++)
        if(FONT_CHARS[i]==ch) found = i;
    return found;
}

// ---------------------------------------------------------------------------
//  Framebuffer (bit c of fb[r] = pixel at row r, column c)
// ---------------------------------------------------------------------------
static uint8_t fb[7];

// scroll position -- runs on its own faster tick (SCROLL_MS), one column/step
static uint32_t g_sp = 0;

#define MSG_N     ((int)(sizeof(MSG)-1))   // characters
#define MSG_COLS  (MSG_N * 6)              // 5 columns + 1 gap each

static void fb_clear(void){
    uint8_t r; for(r=0;r<7;r++) fb[r]=0;
}
static inline void fb_set(uint8_t r, uint8_t c){ fb[r] |= (uint8_t)(1u<<c); }

// column `i` of the message, 0 outside it
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
#define FX_BTNTEST  14   // diagnostics: shows which button is held
#define FX_COUNT    15

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

    case FX_BTNTEST:                        // diagnostics: which button is held
        if(digitalRead(BTN_FX)  == LOW) fb_set(0,0);   // D1  top-left  = PD7
        if(digitalRead(BTN_SLP) == LOW) fb_set(0,6);   // D7  top-right = PA1
        if(digitalRead(BTN_FX)==LOW && digitalRead(BTN_SLP)==LOW) fb_set(6,6);
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

        if(mask){                                 // drive this row's lit pixels
            drive(0, ANODE_PIN[r], a_st);
            for(c=0;c<7;c++)
                if(mask & (1u<<c)) drive(0, CATH_PIN[c], k_st);
        }

        // The dwell is UNCONDITIONAL, even for a row with nothing lit.
        // A pixel's brightness is dwell/frame_time, so skipping empty rows
        // made the frame time follow the number of lit rows -- and that number
        // changes column by column while the text scrolls (an 'h' column
        // lights 7 rows, a gap column lights none). The whole panel then
        // pumped up and down as the picture changed: that is the flicker.
        // A fixed 7 x DWELL_US frame keeps every pixel at exactly 1/7 duty.
        delayMicroseconds(DWELL_US);

        if(mask){
            drive(0, ANODE_PIN[r], ST_HZ);
            for(c=0;c<7;c++)
                if(mask & (1u<<c)) drive(0, CATH_PIN[c], ST_HZ);
        }
    }
}

// ---------------------------------------------------------------------------
uint8_t  fx     = FX_SCROLL;
uint32_t p      = 0;
uint32_t last   = 0;
uint32_t slast  = 0;
uint8_t  prev1  = 1, prev2 = 1;
uint8_t  asleep = 0;

void setup(){
    // --- 1. GPIO port clocks ------------------------------------------------
    // The Arduino core only switches a GPIO port's clock on when a pin on that
    // port is first used (set_GPIO_Port_Clock in PortNames.c, called from
    // pinMode). This sketch drives the matrix by direct register access, so
    // pinMode() is never called on a PC pin -- and ALL 7 anode lines (PC0..PC6)
    // plus cathode K1 (PC7) live on GPIOC. Without this line GPIOC stays
    // clockless, every write to it is ignored, and the matrix is completely
    // dark. (The ch32v003fun firmware had the same enable in main().)
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD;

    // --- 2. timebase --------------------------------------------------------
    // The core never calls SystemInit() and no SYSCLK_FREQ_* macro is defined,
    // so its SystemCoreClock falls back to 8 MHz. The chip actually runs on its
    // reset clock HSI = 24 MHz (verified: RCC->CFGR0 == 0x00000000, i.e.
    // SW/SWS = HSI with no prescaler, and HSI_VALUE == 24000000).
    // pre_init() had armed SysTick with CMP = 8000000/1000-1, so msTick gained
    // 3 ticks per real millisecond: millis() ran 3x fast (the text blurred past
    // instead of scrolling) and delayMicroseconds() -- which derives nbTicks
    // from SysTick->CMP at runtime -- was 3x short.
    // Every timing function in this core (millis/micros/delay/delayMicroseconds)
    // hangs off SysTick->CMP, so re-arming it for the real clock fixes them all.
    SystemCoreClock = 24000000UL;
    SysTick->CTLR = 0;
    SysTick->CNT  = 0;
    SysTick->CMP  = 24000000UL / 1000UL - 1UL;    // exactly 1 ms per tick
    SysTick->SR   = 0;
    SysTick->CTLR = 0xF;                          // enable | irq | hclk | reload
    // (the SysTick NVIC enable that pre_init() did stays in force)

    all_lines_hz();
    pinMode(BTN_FX,  INPUT_PULLUP);   // PD7
    pinMode(BTN_SLP, INPUT_PULLUP);   // PA1

    last = slast = millis();
}

void loop(){
    if(!asleep){
        build_frame(fx, p);
        render_grid();

        uint32_t now = millis();
        if((uint32_t)(now - last) >= STEP_MS){ last = now; p++; }
        if((uint32_t)(now - slast) >= SCROLL_MS){ slast = now; g_sp++; }
    } else {
        // sleep: display fully off (all lines high-Z), poll for wake
        all_lines_hz();
        delay(20);
    }

    uint8_t b1 = (digitalRead(BTN_FX)  == LOW);
    uint8_t b2 = (digitalRead(BTN_SLP) == LOW);

    if(b1 && !prev1 && !asleep){          // BTN: next effect
        fx = (uint8_t)((fx + 1) % FX_COUNT);
        p  = 0;
        g_sp = 0;
        last = slast = millis();
    }
    if(b2 && !prev2){                     // BTN: sleep / wake
        asleep = !asleep;
        if(!asleep){ p = 0; g_sp = 0; last = slast = millis(); }
    }
    prev1 = b1; prev2 = b2;
}
