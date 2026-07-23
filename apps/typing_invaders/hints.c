#include "hints.h"

bool hints_chord(char c, int *first, int *second)
{
    if (c < 'a' || c > 'y') return false;
    int i = c - 'a';
    *first  = i / 5;
    *second = i % 5;
    return true;
}
