/* CtrError_WrapText lays the data-error screen out for the 40-column bottom
 * console: no row reaches the console's own wrap, words stay whole, text rows
 * never touch, and the longest real messages fit the 30-row screen. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "3ds_data_error_text.h"

struct Screen
{
    char rows[64][CTR_ERROR_COLUMNS + 1];
    int count;
};

static void Row(void *ctx, const char *text, size_t length)
{
    struct Screen *s = ctx;
    assert(s->count < 64);
    assert(length <= CTR_ERROR_COLUMNS);
    memcpy(s->rows[s->count], text, length);
    s->rows[s->count][length] = 0;
    s->count++;
}

/* The screen exactly as CtrPlatform_ShowDataError draws it. */
static void Layout(struct Screen *s, const char *title, const char *detail)
{
    memset(s, 0, sizeof(*s));
    CtrError_WrapText("twinembers data error", Row, s);
    CtrError_WrapText(title, Row, s);
    CtrError_WrapText(detail, Row, s);
    CtrError_WrapText("Press A to exit.", Row, s);
}

static void CheckScreen(const struct Screen *s)
{
    /* The final row's newline must not scroll the console. */
    assert(s->count < CTR_ERROR_ROWS);
    for (int i = 0; i + 1 < s->count; i++)
        assert(s->rows[i][0] == 0 || s->rows[i + 1][0] == 0);
    for (int i = 0; i < s->count; i++)
    {
        size_t n = strlen(s->rows[i]);
        assert(n <= CTR_ERROR_COLUMNS);
        assert(n == 0 || s->rows[i][n - 1] != ' ');
    }
}

int main(void)
{
    struct Screen s;

    /* Missing pack: the two help lines are separated, the path keeps its indent. */
    Layout(&s, "twinembers data pack missing.",
           "Use twinembers Builder with your own\n"
           "Pokemon FireRed ROM to create it, then\n"
           "install it as:\n\n"
           "  /3ds/twinembers/twinembers.pak");
    CheckScreen(&s);
    assert(!strcmp(s.rows[4], "Use twinembers Builder with your own"));
    assert(s.rows[5][0] == 0);
    assert(!strcmp(s.rows[6], "Pokemon FireRed ROM to create it, then"));
    assert(!strcmp(s.rows[8], "install it as:"));
    assert(!strcmp(s.rows[10], "  /3ds/twinembers/twinembers.pak"));
    assert(!strcmp(s.rows[12], "Press A to exit."));
    assert(s.count == 14);

    /* A 41-column line no longer leaves its full stop alone on the next row. */
    Layout(&s, "The game data could not be loaded.",
           "The data pack does not match this engine.\nRun the builder again.");
    CheckScreen(&s);
    assert(!strcmp(s.rows[4], "The data pack does not match this"));
    assert(!strcmp(s.rows[6], "engine."));
    assert(!strcmp(s.rows[8], "Run the builder again."));

    /* The longest message in the engine still fits, with whole words only. */
    Layout(&s, "FireRed graphics could not be loaded.",
           "A graphics payload is missing, damaged, or does not match this engine. Rebuild the data pack.");
    CheckScreen(&s);
    assert(!strcmp(s.rows[4], "A graphics payload is missing, damaged,"));
    assert(!strcmp(s.rows[6], "or does not match this engine. Rebuild"));
    assert(!strcmp(s.rows[8], "the data pack."));

    /* A word longer than a row is split rather than overflowing. */
    Layout(&s, "x", "/3ds/twinembers/an/unusually/long/path/without/any/spaces.pak");
    CheckScreen(&s);
    assert(strlen(s.rows[4]) == CTR_ERROR_COLUMNS);

    printf("PASS data error text layout\n");
    return 0;
}
