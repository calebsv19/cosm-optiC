#ifndef MENU_NUMERIC_MODEL_H
#define MENU_NUMERIC_MODEL_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    int min, max;
    int step;       /* Drag/nudge step in stored units; typing bypasses this. */
    int multiple;   /* Mandatory storage constraint, including typed values. */
    int divisor;   /* Stored units per displayed unit. */
    int decimals;
} MenuNumericSpec;

typedef struct {
    bool active, invalid, ownsTextInput;
    int *target;
    MenuNumericSpec spec;
    char text[64];
    size_t cursor, anchor;
    unsigned int blinkEpoch;
} MenuNumericEdit;

int menu_numeric_normalize(MenuNumericSpec spec, double value, bool snap);
bool menu_numeric_parse(MenuNumericSpec spec, const char *text, int *value);
void menu_numeric_format(MenuNumericSpec spec, int value, char *text, size_t size);
void menu_numeric_begin(MenuNumericEdit *edit, int *target, MenuNumericSpec spec);
bool menu_numeric_insert(MenuNumericEdit *edit, const char *text);
void menu_numeric_move(MenuNumericEdit *edit, int position, bool extend);
void menu_numeric_delete(MenuNumericEdit *edit, bool forward);

#endif
