// hints.h - letter -> two-press chord on the fw2kb LOWER page.
// Buttons: 0=GRAY 1=YELLOW 2=GREEN 3=BLUE 4=RED. First press picks the group
// (abcde|fghij|klmno|pqrst|uvwxy), second picks the letter within the group.
// 'z' lives on another page and is never used by the game.
#ifndef HINTS_H
#define HINTS_H
#include <stdbool.h>

bool hints_chord(char c, int *first, int *second);

#endif
