#ifndef CTR_DATA_ERROR_TEXT_H
#define CTR_DATA_ERROR_TEXT_H

/* Layout of the data-error screen on the 40-column bottom console. The console
 * wraps a full row by itself, mid-word, and can leave a single character alone on
 * the next row; every message is therefore word-wrapped to CTR_ERROR_COLUMNS here.
 * The 8x8 console font has no line spacing, so each text row is followed by an
 * empty row and two lines are never drawn directly against each other. Explicit
 * empty lines in a message are dropped: that spacing already exists. Shared with
 * the host test. */

#include <stddef.h>
#include <string.h>

#define CTR_ERROR_COLUMNS 39
#define CTR_ERROR_ROWS 30

typedef void (*CtrErrorRowFn)(void *ctx, const char *text, size_t length);

/* Emit one screen row at a time (length 0 = empty row) for text whose lines are
 * separated by '\n'. A word longer than a row is split at the row width. Leading
 * spaces of a line (indentation) are kept; spaces at a wrap point are dropped. */
static inline void CtrError_WrapText(const char *text, CtrErrorRowFn emit, void *ctx)
{
    while (text != NULL)
    {
        const char *end = strchr(text, '\n');
        size_t len = end != NULL ? (size_t)(end - text) : strlen(text);
        size_t pos = 0;

        while (pos < len)
        {
            size_t take = len - pos;
            if (take > CTR_ERROR_COLUMNS)
            {
                size_t cut = CTR_ERROR_COLUMNS;
                while (cut > 0 && text[pos + cut] != ' ')
                    cut--;
                take = cut > 0 ? cut : CTR_ERROR_COLUMNS;
            }
            emit(ctx, text + pos, take);
            emit(ctx, "", 0);
            pos += take;
            while (pos < len && text[pos] == ' ')
                pos++;
        }
        text = end != NULL ? end + 1 : NULL;
    }
}

#endif
