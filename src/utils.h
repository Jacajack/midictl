#ifndef UTILS_H
#define UTILS_H

#define MIN(a, b) ((a) <= (b) ? (a) : (b))
#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#define CLAMP(x, min, max) (MAX(MIN(x, (max)), (min)))
#define INRANGE(x, min, max) (((x) <= (max)) && ((x) >= (min)))

extern int isempty(const char *s);

#endif