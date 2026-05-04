#include <uriscv/liburiscv.h>

#include "h/print.h"
#include "h/tconst.h"

char *deleteSpaces(char *expr, int len, char *newExpr) {
    for (int i = 0; i < len; i++) {
        if (expr[i] == ' ') {
            continue;
        }
        newExpr[i] = expr[i];
    }
}

int asciiToInt(char *expr, int first, char opr, int second, int len) {
    char *newExpr;
    deleteSpaces(expr, len, newExpr);
    first = newExpr[0] - 48;
    if (newExpr[1] == '+' || newExpr[1] == '-' || newExpr[1] == '*' ||
        newExpr[1] == '/') {
        opr = newExpr[1];
    }
    second = newExpr[2];
}

int calculate(int first, char opr, int second) {}

void main() {
    char buffer[6], operand;
    int first, second;
    int n = SYSCALL(READTERMINAL, (int)buffer, 0, 0);
    asciiToInt(buffer, first, operand, second, n);
    int res = first
}
