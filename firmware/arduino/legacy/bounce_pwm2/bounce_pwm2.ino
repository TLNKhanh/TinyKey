#include <Arduino.h>

#define SPEED_MS      55
#define BRIGHTNESS    15

static const char* MSGS[] = {
    "Nam Kh\xC3\xA1nh",
};
#define MSG_COUNT ((int)(sizeof(MSGS)/sizeof(MSGS[0])))

#define NIB(p)        (0xFu << ((p) * 4))

static const uint8_t K_DPIN[6] = {0, 2, 3, 4, 5, 6};

#define D_COL_MASK     ((1u<<0)|(1u<<2)|(1u<<3)|(1u<<4)|(1u<<5)|(1u<<6))
#define D_COL_MASK_NIB (NIB(0)|NIB(2)|NIB(3)|NIB(4)|NIB(5)|NIB(6))

#define ROW_MASK      0x007Fu
#define COL0_BIT      (1u << 7)

#define DEBOUNCE_MS   20
#define PWM_STEPS     15
#define LMAX          PWM_STEPS
#define PWM_US        16

#define NON_SCROLL_FX 9
#define TOTAL_STEPS   (MSG_COUNT + NON_SCROLL_FX)

#define BTN1_PIN       PD7
#define BTN2_PIN       PA1
#define BTN1_PIN_NIB   NIB(7)
#define BTN2_PIN_NIB   NIB(1)
#define BTN1_OUTDR_BIT (1u << 7)
#define BTN2_OUTDR_BIT (1u << 1)

static uint8_t  fb[7];
static uint8_t  px[7][7];
static uint8_t  disp[7][7];
static uint16_t g_msg_cols[MSG_COUNT];

static uint32_t current_step = 0;
static uint32_t current_p    = 0;
static uint32_t lastFrame    = 0;
static uint32_t lastBlend    = 0;

static const uint8_t FACE_HAPPY[7]       = {0x00,0x22,0x55,0x00,0x22,0x1C,0x00};
static const uint8_t FACE_HAPPY_BLINK[7] = {0x00,0x00,0x77,0x00,0x22,0x1C,0x00};
static const uint8_t FACE_ANGRY[7]       = {0x00,0x41,0x22,0x41,0x00,0x22,0x1C};
static const uint8_t FACE_ANGRY_BLINK[7] = {0x00,0x00,0x77,0x00,0x00,0x22,0x1C};

static uint32_t face_blink_at  = 0;
static uint32_t face_blink_end = 0;
static uint32_t face_rng       = 0x1D2C3B4Au;

static uint32_t face_rand() {
    face_rng ^= face_rng << 13;
    face_rng ^= face_rng >> 17;
    face_rng ^= face_rng << 5;
    return face_rng;
}

static void allOff() {
    GPIOC->OUTDR = ROW_MASK;
    GPIOD->OUTDR &= ~D_COL_MASK;
}

static void renderFrame() {
    for (uint8_t r = 0; r < 7; r++) {
        uint32_t rowC = ROW_MASK & ~(1u << r);
        for (uint8_t s = 0; s < PWM_STEPS; s++) {
            uint32_t outC = rowC;
            if (disp[r][0] > s) outC |= COL0_BIT;
            uint32_t outD = 0;
            for (uint8_t c = 1; c < 7; c++)
                if (disp[r][c] > s) outD |= (1u << K_DPIN[c - 1]);
            GPIOC->OUTDR = outC;
            GPIOD->OUTDR = (GPIOD->OUTDR & ~D_COL_MASK) | outD;
            delayMicroseconds(PWM_US);
        }
        GPIOC->OUTDR = ROW_MASK;
        GPIOD->OUTDR &= ~D_COL_MASK;
    }
}

static void blendFrame() {
    for (uint8_t r = 0; r < 7; r++)
        for (uint8_t c = 0; c < 7; c++) {
            uint8_t tgt = (fb[r] & (1u << c)) ? LMAX : 0;
            px[r][c]    = (uint8_t)((px[r][c] + 3u * tgt + 2u) >> 2);
            disp[r][c]  = (uint8_t)(((uint16_t)px[r][c] * BRIGHTNESS + (LMAX / 2)) / LMAX);
        }
}

static const char FONT_CHARS[] = " !'-.,:?0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static const uint8_t FONT[] = {
    0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x5F,0x00,0x00, 0x00,0x07,0x00,0x00,0x00,
    0x08,0x08,0x08,0x08,0x08, 0x00,0x60,0x60,0x00,0x00, 0x00,0x60,0x20,0x00,0x00,
    0x00,0x36,0x36,0x00,0x00, 0x02,0x01,0x51,0x09,0x06, 0x3E,0x41,0x49,0x41,0x3E,
    0x00,0x42,0x7F,0x40,0x00, 0x42,0x61,0x51,0x49,0x46, 0x21,0x41,0x45,0x4B,0x31,
    0x18,0x14,0x12,0x7F,0x10, 0x27,0x45,0x45,0x45,0x39, 0x3C,0x4A,0x49,0x49,0x30,
    0x01,0x71,0x09,0x05,0x03, 0x36,0x49,0x49,0x49,0x36, 0x06,0x49,0x49,0x29,0x1E,
    0x7E,0x09,0x09,0x09,0x7E, 0x7F,0x49,0x49,0x49,0x36, 0x3E,0x41,0x41,0x41,0x22,
    0x7F,0x41,0x41,0x22,0x1C, 0x7F,0x49,0x49,0x49,0x41, 0x7F,0x09,0x09,0x09,0x01,
    0x3E,0x41,0x49,0x49,0x7B, 0x7F,0x08,0x08,0x08,0x7F, 0x00,0x41,0x7F,0x41,0x00,
    0x20,0x40,0x41,0x7F,0x01, 0x7F,0x08,0x14,0x22,0x41, 0x7F,0x40,0x40,0x40,0x40,
    0x7F,0x02,0x0C,0x02,0x7F, 0x7F,0x04,0x08,0x10,0x7F, 0x3E,0x41,0x41,0x41,0x3E,
    0x7F,0x09,0x09,0x09,0x06, 0x3E,0x41,0x51,0x21,0x5E, 0x7F,0x09,0x19,0x29,0x46,
    0x46,0x49,0x49,0x49,0x31, 0x01,0x01,0x7F,0x01,0x01, 0x3F,0x40,0x40,0x40,0x3F,
    0x1F,0x20,0x40,0x20,0x1F, 0x3F,0x40,0x38,0x40,0x3F, 0x63,0x14,0x08,0x14,0x63,
    0x07,0x08,0x70,0x08,0x07, 0x61,0x51,0x49,0x45,0x43, 0x20,0x54,0x54,0x54,0x78,
    0x7F,0x48,0x44,0x44,0x38, 0x38,0x44,0x44,0x44,0x20, 0x38,0x44,0x44,0x48,0x7F,
    0x38,0x54,0x54,0x54,0x18, 0x08,0x7E,0x09,0x01,0x02, 0x08,0x14,0x54,0x54,0x3C,
    0x7F,0x08,0x04,0x04,0x78, 0x00,0x44,0x7D,0x40,0x00, 0x20,0x40,0x44,0x3D,0x00,
    0x7F,0x10,0x28,0x44,0x00, 0x00,0x00,0x7F,0x00,0x00, 0x7C,0x04,0x18,0x04,0x78,
    0x7C,0x08,0x04,0x04,0x78, 0x38,0x44,0x44,0x44,0x38, 0x7C,0x14,0x14,0x14,0x08,
    0x08,0x14,0x14,0x18,0x7C, 0x7C,0x08,0x04,0x04,0x08, 0x48,0x54,0x54,0x54,0x20,
    0x04,0x3F,0x44,0x40,0x20, 0x3C,0x40,0x40,0x20,0x7C, 0x1C,0x20,0x40,0x20,0x1C,
    0x3C,0x40,0x30,0x40,0x3C, 0x44,0x28,0x10,0x28,0x44, 0x0C,0x50,0x50,0x50,0x3C,
    0x44,0x64,0x54,0x4C,0x44
};

static const uint8_t GLYPH_A_GRAVE[5] = {0x20,0x55,0x56,0x54,0x78};
static const uint8_t GLYPH_A_ACUTE[5] = {0x20,0x56,0x55,0x54,0x78};

static int font_index(uint8_t ch) {
    for (int i = 0; FONT_CHARS[i]; i++)
        if ((uint8_t)FONT_CHARS[i] == ch) return i;
    return 7;
}

static int utf8_len(const char* s) {
    int n = 0, i = 0;
    while (s[i]) { i += ((uint8_t)s[i] == 0xC3) ? 2 : 1; n++; }
    return n;
}

static int utf8_byte_offset(const char* s, int ch_idx) {
    int i = 0;
    while (s[i] && ch_idx > 0) {
        i += ((uint8_t)s[i] == 0xC3) ? 2 : 1;
        ch_idx--;
    }
    return i;
}

static uint8_t str_col(const char *s, int len6, int i) {
    if (i < 0 || i >= len6) return 0;
    int ch = i / 6, k = i % 6;
    if (k == 5) return 0;
    int off = utf8_byte_offset(s, ch);
    if (s[off] == 0) return 0;
    if ((uint8_t)s[off] == 0xC3) {
        uint8_t b2 = (uint8_t)s[off + 1];
        if (b2 == 0)    return 0;
        if (b2 == 0xA0) return GLYPH_A_GRAVE[k];
        if (b2 == 0xA1) return GLYPH_A_ACUTE[k];
        return 0;
    }
    return FONT[font_index((uint8_t)s[off]) * 5 + k];
}

static const int8_t WAVE[16] = {0,1,2,2,3,2,2,1,0,-1,-2,-2,-3,-2,-2,-1};

static void face_render(const uint8_t *normal, const uint8_t *blink, uint32_t p) {
    if (p == 0) {
        face_rng       = 0x1D2C3B4Au ^ millis();
        face_blink_end = 0;
        face_blink_at  = 12 + (face_rand() % 36);
    }
    if (p >= face_blink_at) {
        face_blink_end = p + 2;
        face_blink_at  = p + 12 + (face_rand() % 36);
    }
    const uint8_t *f = (p < face_blink_end) ? blink : normal;
    for (uint8_t r = 0; r < 7; r++) fb[r] = f[r];
}

static void build_frame(uint8_t step, uint32_t p) {
    uint8_t r, c;
    for (r = 0; r < 7; r++) fb[r] = 0;

    if (step < MSG_COUNT) {
        int len6 = (int)g_msg_cols[step];
        uint32_t pos = (p + 6) % ((uint32_t)len6 + 7u);
        for (c = 0; c < 7; c++) {
            uint8_t col = str_col(MSGS[step], len6, (int)pos + (int)c - 6);
            for (r = 0; r < 7; r++) if (col & (1u << r)) fb[r] |= (uint8_t)(1u << c);
        }
        return;
    }

    uint8_t fx = (uint8_t)(step - MSG_COUNT);
    switch (fx) {
    case 0:
        for (c = 0; c < 7; c++) {
            int row = 3 + WAVE[(c + (int)p) & 15];
            if (row     >= 0 && row     < 7) fb[row]     |= (uint8_t)(1u << c);
            if (row + 1 >= 0 && row + 1 < 7) fb[row + 1] |= (uint8_t)(1u << c);
        }
        break;

    case 1:
        for (c = 0; c < 7; c++) {
            int drop = (int)((p / 2 + (uint32_t)(c * 5)) % 12) - 4;
            for (int k = 0; k < 3; k++) {
                int rr = drop - k;
                if (rr >= 0 && rr < 7) fb[rr] |= (uint8_t)(1u << c);
            }
        }
        break;

    case 2: {
        static const uint8_t BIG[7] = {0x36,0x7F,0x7F,0x3E,0x1C,0x08,0x00};
        static const uint8_t SM[7]  = {0x00,0x36,0x3E,0x1C,0x08,0x00,0x00};
        const uint8_t *h = ((p / 6) & 1) ? BIG : SM;
        for (r = 0; r < 7; r++) fb[r] = h[r];
        break;
    }

    case 3: face_render(FACE_HAPPY, FACE_HAPPY_BLINK, p); break;
    case 4: face_render(FACE_ANGRY, FACE_ANGRY_BLINK, p); break;

    case 5: {
        uint32_t rng = 0x9E3779B9u ^ (p * 1103515245u);
        for (int i = 0; i < 10; i++) {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            int x = (int)(rng % 7);
            int y = (int)((rng >> 8) % 7);
            fb[y] |= (uint8_t)(1u << x);
        }
        break;
    }

    case 6: {
        int t = (int)(p & 7);
        int phase = (t < 4) ? t : 7 - t;
        for (r = 0; r < 7; r++) {
            for (c = 0; c < 7; c++) {
                int dr = (r < 3) ? (3 - r) : (r - 3);
                int dc = (c < 3) ? (3 - c) : (c - 3);
                int cheb = (dr > dc) ? dr : dc;
                if (cheb == phase) fb[r] |= (uint8_t)(1u << c);
            }
        }
        break;
    }

    case 7:
        for (r = 0; r < 7; r++)
            for (c = 0; c < 7; c++)
                if (((r + c + (int)p) & 3) == 0) fb[r] |= (uint8_t)(1u << c);
        break;

    case 8: {
        static const uint8_t ARR[7] = {0x38, 0x1C, 0x0E, 0x07, 0x0E, 0x1C, 0x38};
        int x = 7 - (int)(p % 15);
        for (r = 0; r < 7; r++) {
            uint8_t v = ARR[r];
            if (x >= 0) fb[r] = (uint8_t)((v <<  x) & 0x7F);
            else        fb[r] = (uint8_t)((v >> -x) & 0x7F);
        }
        break;
    }
    }
}

static inline uint8_t readB1() { return (GPIOD->INDR & BTN1_OUTDR_BIT) ? 0 : 1; }
static inline uint8_t readB2() { return (GPIOA->INDR & BTN2_OUTDR_BIT) ? 0 : 1; }

static void wake_isr() { }

static void enterSleepMode() {
    allOff();
    while (readB2()) {
        delay(10); 
    }
    delay(50);
    SysTick->CTLR = 0;
    __WFI(); 
    SysTick->CTLR = 0xF;
    lastFrame = millis();
    lastBlend = millis();
}

void setup() {
    delay(100); 
    RCC->APB2PCENR |= (1u << 0) | (1u << 2) | (1u << 4) | (1u << 5);
    if ((SysTick->CTLR & 0x1u) == 0) {
        SystemCoreClock = 24000000UL;
        SysTick->CTLR = 0;
        SysTick->CNT  = 0;
        SysTick->CMP  = SystemCoreClock / 1000UL - 1UL;
        SysTick->SR   = 0;
        SysTick->CTLR = 0xF;
    }
    AFIO->PCFR1 &= ~(1u << 15);
    GPIOC->CFGLR = 0x22222222u;
    GPIOD->CFGLR = (GPIOD->CFGLR & ~D_COL_MASK_NIB) | (0x22222222u & D_COL_MASK_NIB);
    GPIOD->CFGLR = (GPIOD->CFGLR & ~BTN1_PIN_NIB) | (0x8u << (7 * 4));
    GPIOD->OUTDR |= BTN1_OUTDR_BIT;
    GPIOA->CFGLR = (GPIOA->CFGLR & ~BTN2_PIN_NIB) | (0x8u << (1 * 4));
    GPIOA->OUTDR |= BTN2_OUTDR_BIT;
    for (int i = 0; i < MSG_COUNT; i++)
        g_msg_cols[i] = (uint16_t)(utf8_len(MSGS[i]) * 6);
    attachInterrupt(BTN2_PIN, GPIO_Mode_IPU, wake_isr, EXTI_Mode_Interrupt, EXTI_Trigger_Falling);
    lastFrame = millis();
    lastBlend = millis();
    allOff();
}

void loop() {
    static uint8_t  b1prev    = 0, b2prev = 0;
    static uint32_t b1lock    = 0, b2lock = 0;
    uint32_t ms = millis();
    uint8_t  b1 = readB1();
    uint8_t  b2 = readB2();
    if (b2 && !b2prev && (uint32_t)(ms - b2lock) >= DEBOUNCE_MS) {
        b2lock = ms;
        enterSleepMode(); 
    }
    b2prev = b2;
    if (b1 && !b1prev && (uint32_t)(ms - b1lock) >= DEBOUNCE_MS) {
        b1lock = ms;
        current_step = (current_step + 1) % TOTAL_STEPS;
        current_p = 0;
        lastFrame = ms - SPEED_MS;
        lastBlend = ms - 15u;
    }
    b1prev = b1;
    if ((uint32_t)(ms - lastFrame) >= SPEED_MS) {
        lastFrame = ms;
        build_frame(current_step, current_p);
        current_p++;
    }
    if ((uint32_t)(ms - lastBlend) >= 15u) {
        lastBlend = ms;
        blendFrame();
    }
    renderFrame();
}