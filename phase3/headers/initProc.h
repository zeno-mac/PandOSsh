#include "../../headers/types.h"

#define SHELL_ASID 1

extern int masterSemaphore;
extern int shellSemaphore;

extern support_t supportPool[UPROCMAX];

int createUProc(int asid);
