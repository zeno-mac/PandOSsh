#include <uriscv/liburiscv.h>

#include "h/print.h"
#include "h/tconst.h"

void deleteSpaces(char *expr, int len, char *newExpr) {
    int index = 0;
    for (int i = 0; i < len; i++) {
        if (expr[i] == ' ') {
            continue;
        }
        newExpr[index] = expr[i];
        index++;
    }
    newExpr[index] = '\0';
}

int asciiToInt(char *expr, int *first, char *opr, int *second, int len) {
    char newExpr[10];
    deleteSpaces(expr, len, newExpr);
    *first = newExpr[0] - 48;
    if (*first < 0 || *first > 9) {
        char *input = "first input not a number\n";
        SYSCALL(WRITETERMINAL, (int)input, 25, 0);
        return -1;
    }
    if (newExpr[1] == '+' || newExpr[1] == '-' || newExpr[1] == '*' ||
        newExpr[1] == '/') {
        *opr = newExpr[1];
    } else {
        char *error = "not an allowed operand\n";
        SYSCALL(WRITETERMINAL, (int)error, 23, 0);
        return -1;
    }

    *second = newExpr[2] - 48;
    if (*second < 0 || *second > 9) {
        char *input = "second input not a number\n";
        SYSCALL(WRITETERMINAL, (int)input, 26, 0);
        return -1;
    }
    return 1;
}

int calculate(int num1, char opr, int num2) {
    int result = 0;
    switch (opr) {
    case '+':
        result = num1 + num2;
        break;
    case '-':
        result = num1 - num2;
        break;
    case '*':
        result = num1 * num2;
        break;
    case '/':
        if (num2 == 0) {
            char *error = "division by 0 not allowed\n";
            SYSCALL(WRITETERMINAL, (int)error, 26, 0);
            SYSCALL(TERMINATE, 0, 0, 0);
        } else {
            result = num1 / num2;
        }
        break;
    default:
        SYSCALL(TERMINATE, 0, 0, 0);
        break;
    }
    return result;
}

void main() {
    char buffer[10], operand;
    int first, second;
    char *input = "insert expression\n";
    SYSCALL(WRITETERMINAL, (int)input, 18, 0);
    int n = SYSCALL(READTERMINAL, (int)buffer, 0, 0);
    int retval = asciiToInt(buffer, &first, &operand, &second, n);
    if (retval == -1) {
        SYSCALL(TERMINATE, 0, 0, 0);
    }
    int result = calculate(first, operand, second);
    char res_buffer[3];
    if (result < 0) {
        res_buffer[0] = '-';
        res_buffer[1] = -result + 48;
    } else {
        int dec = result / 10;
        int unit = result % 10;

        res_buffer[0] = dec + 48;
        res_buffer[1] = unit + 48;
    }

    res_buffer[2] = '\n';
    SYSCALL(WRITETERMINAL, (int)res_buffer, 3, 0);
    SYSCALL(TERMINATE, 0, 0, 0);
}

void main2() {
    char *input1 = "insert first number\n";
    SYSCALL(WRITETERMINAL, (int)input1, 20, 0);
    char first[4];
    SYSCALL(READTERMINAL, (int)first, 0, 0);
    int num1 = first[0] - 48;
    char *input2 = "insert operand + - * /\n";
    SYSCALL(WRITETERMINAL, (int)input2, 23, 0);
    char operand[4];
    SYSCALL(READTERMINAL, (int)operand, 0, 0);
    char *input3 = "insert second number\n";
    SYSCALL(WRITETERMINAL, (int)input3, 21, 0);
    char second[4];
    SYSCALL(READTERMINAL, (int)second, 0, 0);
    int num2 = second[0] - 48;
    int result = 0;
    switch (operand[0]) {
    case '+':
        result = num1 + num2;
        break;
    case '-':
        result = num1 - num2;
        break;
    case '*':
        result = num1 * num2;
        break;
    case '/':
        if (num2 == 0) {
            char *error = "division by 0 not allowed\n";
            SYSCALL(WRITETERMINAL, (int)error, 26, 0);
        } else {
            result = num1 / num2;
        }
        break;
    default:
        char *error = "not an allowed operand\n";
        SYSCALL(WRITETERMINAL, (int)error, 23, 0);
        break;
    }
    char res_buffer[3];
    if (result < 0) {
        res_buffer[0] = '-';
        res_buffer[1] = -result + 48;
    } else {
        int dec = result / 10;
        int unit = result % 10;

        res_buffer[0] = dec + 48;
        res_buffer[1] = unit + 48;
    }

    res_buffer[2] = '\n';
    SYSCALL(WRITETERMINAL, (int)res_buffer, 3, 0);
    SYSCALL(TERMINATE, 0, 0, 0);
}
