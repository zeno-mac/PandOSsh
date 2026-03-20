#include "../headers/initial.h"
#include "../headers/scheduler.h"
#include "../headers/exceptions.h"
#include "../../headers/types.h"
#include "../../headers/const.h"
#include "../../headers/listx.h"

#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>

#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/asl.h"

int nsys1(int a1, int a2, int a3);
void nsys2(int a1, state_t *excState);
void nsys3(int a1, state_t *excState);
void nsys4(int a1, state_t *excState);
void nsys5(int a1, int a2, state_t *excState);
void nsys6(state_t *excState);
void nsys7(state_t *excState);
void nsys8(state_t *excState);
void nsys9(int a1, state_t *excState);
void nsys10(state_t *excState);
