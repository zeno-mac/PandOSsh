#include "../../headers/types.h"

#define SHELL_ASID 1

extern int masterSemaphore;
extern int shellSemaphore;

extern support_t supportPool[UPROCMAX];

void initUProc(int asid, state_t *state, support_t *support);
int createUProc(int asid);
