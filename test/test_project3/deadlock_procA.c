#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mailbox.h>

#include <assert.h>

#define MBOX_NAME1 "mbox1"
#define MBOX_NAME2 "mbox2"
#define MSG_SIZE 64

static int print_location_send = 2;
static int print_location_recv = 3;
static char blank[] = {"                                                  "};

void myPrintf(char *str, int print_location) {
    sys_move_cursor(0, print_location);
    printf("%s\n", blank);
    sys_move_cursor(0, print_location);
    printf("%s", str);
}

void *send(void *arg) {
    int *mbox_p = (int *)arg;
    char send_buf[MSG_SIZE];
    int send_length;

    while(1) {
        send_length = rand() % MSG_SIZE + 1;
        generateRandomString(send_buf, send_length);
        myPrintf("[Thread A-send] Trying to send to mbox1...\n", print_location_send);
        sys_mbox_send(*mbox_p, send_buf, send_length);
        myPrintf("[Thread A-send] Sent to mbox1               \n", print_location_send);
        int sleep_time = rand() % 3 + 1;
        sys_sleep(sleep_time);
    }

    sys_thread_exit();
    return NULL;
}

void *recv(void *arg) {
    int *mbox_p = (int *)arg;
    char recv_buf[MSG_SIZE];
    int recv_length;

    while(1) {
        recv_length = rand() % MSG_SIZE + 1;
        myPrintf("[Thread A-recv] Trying to recv from mbox2...\n", print_location_recv);
        sys_mbox_recv(*mbox_p, recv_buf, recv_length);
        myPrintf("[Thread A-recv] Received from mbox2          \n", print_location_recv);
        int sleep_time = rand() % 3 + 1;
        sys_sleep(sleep_time);
    }

    sys_thread_exit();
    return NULL;
}

int main() {
    sys_move_cursor(0, print_location_send);
    printf("[Process A] Starting...\n");
    
    int mbox1 = sys_mbox_open(MBOX_NAME1);
    int mbox2 = sys_mbox_open(MBOX_NAME2);

    tid_t tid_send = sys_thread_create((void *)send, (void *)&mbox1);
    tid_t tid_recv = sys_thread_create((void *)recv, (void *)&mbox2);

    sys_thread_join(tid_send);
    sys_thread_join(tid_recv);
    
    sys_mbox_close(mbox1);
    sys_mbox_close(mbox2);
    sys_move_cursor(0, print_location_send);
    printf("%s\n", blank);
    sys_move_cursor(0, print_location_send);
    printf("[Process A] Finished\n");
    return 0;
}


// #include <unistd.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <mailbox.h>

// #define MBOX_NAME1 "mbox1"
// #define MBOX_NAME2 "mbox2"
// #define MSG_SIZE 64

// static int print_location = 2;
// static char blank[] = {"                                             "};

// void myPrintf(char *str) {
//     sys_move_cursor(0, print_location);
//     printf("%s\n", blank);
//     sys_move_cursor(0, print_location);
//     printf("%s", str);
// }

// int main() {
//     sys_move_cursor(0, print_location);
//     printf("[Process A] Starting...\n");
    
//     int mbox1 = sys_mbox_open(MBOX_NAME1);
//     int mbox2 = sys_mbox_open(MBOX_NAME2);

//     char send_buf[MSG_SIZE];
//     int send_length;
//     char recv_buf[MSG_SIZE];
//     int recv_length;

//     for(int i = 0; i < 10; i++) {
//         send_length = rand() % MSG_SIZE + 1;
//         generateRandomString(send_buf, send_length);
//         myPrintf("[Process A] Trying to send to mbox1...\n");
//         sys_mbox_send(mbox1, send_buf, send_length);
//         myPrintf("[Process A] Sent to mbox1\n");

//         recv_length = rand() % MSG_SIZE + 1;
//         myPrintf("[Process A] Trying to recv from mbox2...\n");
//         sys_mbox_recv(mbox2, recv_buf, recv_length);
//         myPrintf("[Process A] Received from mbox2\n");
//     }
    
//     sys_mbox_close(mbox1);
//     sys_mbox_close(mbox2);
//     sys_move_cursor(0, print_location);
//     printf("%s\n", blank);
//     sys_move_cursor(0, print_location);
//     printf("[Process A] Finished\n");
//     return 0;
// }