#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mailbox.h>

#define MBOX_NAME1 "mbox1"
#define MBOX_NAME2 "mbox2"
#define MSG_SIZE 64

static int print_location = 1;
static char blank[] = {"                                                          "};

void init_mailboxes(int mbox1, int mbox2) {
    char fill_msg[MSG_SIZE];
    memset(fill_msg, 'X', MSG_SIZE);

    sys_mbox_send(mbox1, fill_msg, MSG_SIZE);
    sys_mbox_send(mbox2, fill_msg, MSG_SIZE);
}

int main(int argc, char *argv[]) {
    sys_move_cursor(0, print_location);
    printf("[Main] Starting deadlock test...\n");
    
    int mbox1 = sys_mbox_open(MBOX_NAME1);
    int mbox2 = sys_mbox_open(MBOX_NAME2);

    init_mailboxes(mbox1, mbox2);
    
    pid_t pid_a = sys_exec("deadlock_procA", 0, NULL);
    pid_t pid_b = sys_exec("deadlock_procB", 0, NULL);
    
    sys_move_cursor(0, print_location);
    printf("%s\n", blank);
    sys_move_cursor(0, print_location);
    printf("[Main] Both processes started\n");

    sys_waitpid(pid_a);
    sys_waitpid(pid_b);

    sys_mbox_close(mbox1);
    sys_mbox_close(mbox2);
    
    return 0;
}