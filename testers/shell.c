#include <uriscv/liburiscv.h>

#include "../headers/const.h"
#include "h/print.h"
#include "h/tconst.h"

struct mapAsid {
    char *name;
    int asid;
};

static struct mapAsid arr[] = {
    {"fibEight", 2},
    {"echo", 3},
    {"fibEleven", 4},
    {"uname", 5},
    {"date", 6},
    {"sl", 7},
    {"calc", 8},
};

void trimSpaces(char *s) {
    int start = 0;
    int end = 0;
    int write = 0;

    /* trova il primo carattere non spazio */
    while (s[start] == ' ') {
        start++;
    }

    /* sposta la stringa a sinistra */
    while (s[start] != '\0') {
        s[write] = s[start];
        start++;
        write++;
    }

    s[write] = '\0';

    /* rimuove gli spazi finali */
    end = 0;
    while (s[end] != '\0') {
        end++;
    }

    while (end > 0 && s[end - 1] == ' ') {
        s[end - 1] = '\0';
        end--;
    }
}

void chkExitAndTerminate(char *buff) {
    if (buff[0] == 'e' && buff[1] == 'x' && buff[2] == 'i' && buff[3] == 't' &&
        buff[4] == '\0') {
        SYSCALL(TERMINATE, 0, 0, 0);
    }
}

void main() {

    char *startPrompt = "> ";
    char *unknownProg = "command not found :<\n";
    char buffer[MAXSTRLENG];
    int found, excAsid;
    while (1) {
        SYSCALL(WRITETERMINAL, (int)startPrompt, 2, 0);
        // n = number of characters actually read
        int n = SYSCALL(READTERMINAL, (int)buffer, 0, 0);
        if (buffer[n - 1] == '\n') {
            buffer[n - 1] = '\0';
        }

        trimSpaces(buffer);

        chkExitAndTerminate(buffer);
        if (buffer[0] == '\0') {
            continue;
        }

        found = 0;
        for (int i = 0; i < 7; i++) { // search in known program asid's
            int index = 0;

            while (arr[i].name[index] == buffer[index]) {
                if (arr[i].name[index] == '\0') {
                    found = 1;
                    excAsid = arr[i].asid;
                    break;
                }
                index++;
            }
            if (found == 1) {
                SYSCALL(EXECUTE, excAsid, 0, 0);
                break;
            }
        }
        if (found == 0) {
            SYSCALL(WRITETERMINAL, (int)unknownProg, 21, 0);
        }
    }
}
