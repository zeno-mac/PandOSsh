#include "../../headers/const.h"
#include "../../headers/types.h"
#include "../../phase2/src/auxfun.c"
#include <uriscv/liburiscv.h>

int masterSemaphore;
int shellSemaphore;

support_t supportPool[UPROCMAX];

void test() {
  int masterSemaphore = 0;
  int shellSemaphore = 0;
}
