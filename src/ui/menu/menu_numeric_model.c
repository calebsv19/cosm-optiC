#include "ui/menu_numeric_model.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int menu_numeric_normalize(MenuNumericSpec spec, double value, bool snap) {
    const int multiple = spec.multiple > 0 ? spec.multiple : 1;
    if (!isfinite(value)) value = spec.min;
    if (value < spec.min) value = spec.min;
    if (value > spec.max) value = spec.max;
    if (snap && spec.step > 1) value = round(value / spec.step) * spec.step;
    value = round(value);
    if (multiple > 1) value = ceil(value / multiple) * multiple;
    if (value < spec.min) value = ceil((double)spec.min / multiple) * multiple;
    if (value > spec.max) value = floor((double)spec.max / multiple) * multiple;
    return (int)value;
}

bool menu_numeric_parse(MenuNumericSpec spec, const char *text, int *value) {
    char *end;
    double parsed;
    if (!text || !value) return false;
    while (isspace((unsigned char)*text)) ++text;
    if (!*text) {
        *value = menu_numeric_normalize(spec, 0, false);
        return true;
    }
    errno = 0;
    parsed = strtod(text, &end);
    if (end == text || errno == ERANGE || !isfinite(parsed)) return false;
    while (isspace((unsigned char)*end)) ++end;
    if (*end || (spec.decimals == 0 && trunc(parsed) != parsed)) return false;
    parsed *= spec.divisor;
    if (!isfinite(parsed)) return false;
    *value = menu_numeric_normalize(spec, parsed, false);
    return true;
}

void menu_numeric_format(MenuNumericSpec spec, int value, char *text, size_t size) {
    snprintf(text, size, "%.*f", spec.decimals, (double)value / spec.divisor);
}

void menu_numeric_begin(MenuNumericEdit *edit, int *target, MenuNumericSpec spec) {
    memset(edit, 0, sizeof(*edit));
    edit->active = true;
    edit->target = target;
    edit->spec = spec;
    menu_numeric_format(spec, *target, edit->text, sizeof(edit->text));
    edit->cursor = strlen(edit->text);
}

/* Cursor/anchor selection follows BehaviorSim's existing text-edit model.
 * This adapter stays menu-local until a shared text-field contract is adopted. */
void menu_numeric_move(MenuNumericEdit *edit, int position, bool extend) {
    const int length = (int)strlen(edit->text);
    if (position < 0) position = 0;
    if (position > length) position = length;
    edit->cursor = (size_t)position;
    if (!extend) edit->anchor = edit->cursor;
}

bool menu_numeric_insert(MenuNumericEdit *edit, const char *text) {
    const size_t start = edit->cursor < edit->anchor ? edit->cursor : edit->anchor;
    const size_t end = edit->cursor > edit->anchor ? edit->cursor : edit->anchor;
    const size_t length = strlen(edit->text);
    const size_t incoming = strlen(text);
    if (length - (end - start) + incoming >= sizeof(edit->text)) return false;
    /* Numeric drafts may be temporarily incomplete, but are always ASCII. */
    for (size_t i = 0; i < incoming; ++i) {
        if (!strchr("0123456789.+-eE ", text[i])) return false;
    }
    memmove(edit->text + start + incoming, edit->text + end, length - end + 1);
    memcpy(edit->text + start, text, incoming);
    edit->cursor = edit->anchor = start + incoming;
    edit->invalid = false;
    return true;
}

void menu_numeric_delete(MenuNumericEdit *edit, bool forward) {
    if (edit->cursor == edit->anchor) {
        if (forward && edit->cursor < strlen(edit->text)) ++edit->anchor;
        if (!forward && edit->cursor > 0) --edit->anchor;
    }
    (void)menu_numeric_insert(edit, "");
}
