#include "ui_font.h"
static const uint8_t glyphs[36][7] = {
    {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},      {14, 17, 1, 2, 4, 8, 31},
    {30, 1, 1, 14, 1, 1, 30},     {2, 6, 10, 18, 31, 2, 2},     {31, 16, 16, 30, 1, 1, 30},
    {14, 16, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},       {14, 17, 17, 14, 17, 17, 14},
    {14, 17, 17, 15, 1, 1, 14},   {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30},
    {14, 17, 16, 16, 16, 17, 14}, {30, 17, 17, 17, 17, 17, 30}, {31, 16, 16, 30, 16, 16, 31},
    {31, 16, 16, 30, 16, 16, 16}, {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17},
    {14, 4, 4, 4, 4, 4, 14},      {7, 2, 2, 2, 18, 18, 12},     {17, 18, 20, 24, 20, 18, 17},
    {16, 16, 16, 16, 16, 16, 31}, {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17},
    {14, 17, 17, 17, 17, 17, 14}, {30, 17, 17, 30, 16, 16, 16}, {14, 17, 17, 17, 21, 18, 13},
    {30, 17, 17, 30, 20, 18, 17}, {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},
    {17, 17, 17, 17, 17, 17, 14}, {17, 17, 17, 17, 17, 10, 4},  {17, 17, 17, 21, 21, 21, 10},
    {17, 17, 10, 4, 10, 17, 17},  {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31},
};

uint8_t ui_glyph_row(char c, unsigned row)
{
    if (row >= 7)
        return 0;
    if (c >= '0' && c <= '9')
        return glyphs[c - '0'][row];
    if (c >= 'A' && c <= 'Z')
        return glyphs[10 + c - 'A'][row];
    if (c == ':')
        return row == 2 || row == 5 ? 4 : 0;
    if (c == '-')
        return row == 3 ? 14 : 0;
    if (c == '>')
        return row == 1 || row == 5 ? 8 : row == 2 || row == 4 ? 4 : row == 3 ? 2 : 0;
    if (c == '/')
        return 1U << (row < 5 ? row : 4);
    return 0;
}

bool ui_text_pixel(int x, int y, int left, int top, unsigned scale, const char *text)
{
    if (x < left || y < top || y >= top + (int)(7 * scale))
        return false;
    unsigned index = (unsigned)(x - left) / (6 * scale), col = (unsigned)(x - left) / scale % 6;
    if (col >= 5)
        return false;
    for (unsigned i = 0; i <= index; ++i)
        if (!text[i])
            return false;
    return (ui_glyph_row(text[index], (unsigned)(y - top) / scale) & (1U << (4 - col))) != 0;
}

void ui_number(char *out, uint32_t value, unsigned digits)
{
    uint32_t limit = 1;
    for (unsigned i = 0; i < digits; ++i)
        limit *= 10;
    if (value >= limit)
        value = limit - 1;
    out[digits] = 0;
    while (digits) {
        out[--digits] = (char)('0' + value % 10);
        value /= 10;
    }
}
