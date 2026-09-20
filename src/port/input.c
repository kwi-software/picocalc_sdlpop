/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pop_port.h"
#include <string.h>
static uint8_t held[512];
static PC_Event queue[64];
static unsigned head, tail, overflows;
static bool reset_pending;
static unsigned scan(uint8_t k) {
    if (k >= 'a' && k <= 'z')
        return k - 'a' + 4;
    if (k >= 'A' && k <= 'Z')
        return k - 'A' + 4;
    if (k >= '1' && k <= '9')
        return k - '1' + 30;
    if (k == '0')
        return 39;
    if (k >= 0x81 && k <= 0x89)
        return k - 0x81 + 58;
    if (k == 0x90)
        return 67;
    switch (k) {
    case 8:
        return 42;
    case 9:
        return 43;
    case 10:
    case 13:
        return 40;
    case 32:
        return 44;
    case 0xb1:
        return 41;
    case 0xb4:
        return 80;
    case 0xb5:
        return 82;
    case 0xb6:
        return 81;
    case 0xb7:
        return 79;
    case 0xa1:
        return 226;
    case 0xa2:
        return 225;
    case 0xa3:
        return 229;
    case 0xa5:
        return 224;
    case 0xd1:
        return 73;
    case 0xd2:
        return 74;
    case 0xd4:
        return 76;
    case 0xd5:
        return 77;
    case 0xd6:
        return 75;
    case 0xd7:
        return 78;
    case 0xc1:
        return 57;
    case '-':
        return 45;
    case '=':
        return 46;
    case '[':
        return 47;
    case ']':
        return 48;
    case '\\':
        return 49;
    case ';':
        return 51;
    case '\'':
        return 52;
    case '`':
        return 53;
    case ',':
        return 54;
    case '.':
        return 55;
    case '/':
        return 56;
    default:
        return 0;
    }
}
static unsigned mods(void) {
    return (held[225] ? 1 : 0) | (held[229] ? 2 : 0) | (held[224] ? 64 : 0) | (held[226] ? 256 : 0);
}
void PC_InputReset(void) {
    memset(held, 0, sizeof held);
    head = tail = overflows = 0;
    reset_pending = false;
}
void PC_InputFeed(uint8_t raw, unsigned state) {
    unsigned sc = scan(raw);
    if (!sc || state < 1 || state > 3)
        return;
    if ((state == 3 && !held[sc]) || (state != 3 && held[sc]))
        return;
    bool repeat = held[sc] != 0;
    held[sc] = (state != 3);
    unsigned next = (head + 1) & 63;
    if (next == tail) {
        overflows++;
        head = tail = 0;
        memset(held, 0, sizeof held);
        reset_pending = true;
        return;
    }
    queue[head] = (PC_Event){.key = raw,
                             .type = state == 3 ? PC_KEYUP : PC_KEYDOWN,
                             .repeat = state == 3 ? 0 : repeat,
                             .scancode = sc,
                             .modifiers = mods()};
    head = next;
}
bool PC_InputNext(PC_Event *e) {
    if (!e)
        return false;
    if (reset_pending) {
        reset_pending = false;
        *e = (PC_Event){.type = PC_INPUT_RESET};
        return true;
    }
    if (tail == head)
        return false;
    *e = queue[tail];
    tail = (tail + 1) & 63;
    return true;
}
const uint8_t *PC_GetKeyboardState(unsigned *n) {
    if (n)
        *n = 512;
    return held;
}
unsigned PC_InputOverflows(void) { return overflows; }
void POP_ApplyKeyEvent(const PC_Event *e, uint8_t *keys, size_t n, int *last, int *any) {
    if (!e || !keys)
        return;
    if (e->type == PC_INPUT_RESET) {
        memset(keys, 0, n);
        if (last)
            *last = 0;
        if (any)
            *any = 0;
        return;
    }
    unsigned s = e->scancode;
    if (s >= n)
        return;
    if (e->type == PC_KEYUP) {
        keys[s] &= (uint8_t)~1u;
        return;
    }
    if (e->type != PC_KEYDOWN || e->repeat)
        return;
    keys[s] |= 3;
    if (any)
        *any = (int)s;
    if (s == 224 || s == 225 || s == 226 || s == 229 || s == 57)
        return;
    if (last)
        *last = (int)s | ((e->modifiers & 3) ? 0x8000 : 0) | ((e->modifiers & 0xc0) ? 0x4000 : 0) |
                ((e->modifiers & 0x300) ? 0x2000 : 0);
}
