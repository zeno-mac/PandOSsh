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
/*
 * Removes leading and trailing spaces from the input string.
 *
 * This allows the shell to accept commands even if the user inserts
 * extra spaces before or after the program name.
 */
void trimSpaces(char *s) {
    int start = 0;
    int end = 0;
    int write = 0;

    /* Find the first non-space character. */
    while (s[start] == ' ') {
        start++;
    }

    /* Shift the useful part of the string to the beginning. */
    while (s[start] != '\0') {
        s[write] = s[start];
        start++;
        write++;
    }

    s[write] = '\0';

    /* Find the end of the trimmed string. */
    end = 0;
    while (s[end] != '\0') {
        end++;
    }

    while (end > 0 && s[end - 1] == ' ') {
        s[end - 1] = '\0';
        end--;
    }
}

/*
 * Checks whether the user typed "exit".
 *
 * If so, the shell terminates itself using the positive TERMINATE syscall.
 * This will also unblock the Instantiator Process through the SYS2 handler.
 */
void chkExitAndTerminate(char *buff) {
    if (buff[0] == 'e' && buff[1] == 'x' && buff[2] == 'i' && buff[3] == 't' &&
        buff[4] == '\0') {
        SYSCALL(TERMINATE, 0, 0, 0);
    }
}

/*
 * Main loop of the user shell.
 *
 * The shell repeatedly:
 * - prints the prompt;
 * - reads a command from terminal;
 * - trims spaces;
 * - checks for the "exit" command;
 * - searches the command in the static name-to-ASID table;
 * - executes the selected program using the EXECUTE syscall.
 *
 * If the command is unknown, an error message is printed.
 */
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
