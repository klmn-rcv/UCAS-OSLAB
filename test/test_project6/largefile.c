#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define LARGE_TARGET   (128 * 1024 * 1024)  // 128MB
#define TAIL_PATTERN   "ABCD"
#define HEAD_PATTERN   "HEAD"

static char buf[8];

static int bytes_equal(const char *a, const char *b, int n)
{
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    int fd = sys_open("big.bin", O_RDWR);
    if (fd < 0) {
        sys_move_cursor(0, 0);
        printf("[largefile] open failed\n");
        return -1;
    }

    // Write a small header at the beginning.
    if (sys_fwrite(fd, HEAD_PATTERN, strlen(HEAD_PATTERN)) != (int)strlen(HEAD_PATTERN)) {
        sys_move_cursor(0, 0);
        printf("[largefile] write head failed\n");
        sys_close(fd);
        return -1;
    }
    else {
        sys_move_cursor(0, 0);
        printf("[largefile] wrote head\n");
    }

    // Seek near 128MB and write tail pattern to create a sparse large file.
    if (sys_lseek(fd, LARGE_TARGET - (int)strlen(TAIL_PATTERN), SEEK_SET) < 0) {
        sys_move_cursor(0, 1);
        printf("[largefile] lseek failed\n");
        sys_close(fd);
        return -1;
    }
    if (sys_fwrite(fd, TAIL_PATTERN, strlen(TAIL_PATTERN)) != (int)strlen(TAIL_PATTERN)) {
        sys_move_cursor(0, 1);
        printf("[largefile] write tail failed\n");
        sys_close(fd);
        return -1;
    }
    else {
        sys_move_cursor(0, 1);
        printf("[largefile] wrote tail\n");
    }

    // Verify tail content.
    if (sys_lseek(fd, LARGE_TARGET - (int)strlen(TAIL_PATTERN), SEEK_SET) < 0) {
        sys_move_cursor(0, 2);
        printf("[largefile] lseek verify failed\n");
        sys_close(fd);
        return -1;
    }

    memset(buf, 0, sizeof(buf));
    if (sys_fread(fd, buf, strlen(TAIL_PATTERN)) != (int)strlen(TAIL_PATTERN)) {
        sys_move_cursor(0, 2);
        printf("[largefile] read tail failed\n");
        sys_close(fd);
        return -1;
    }
    if (!bytes_equal(buf, TAIL_PATTERN, (int)strlen(TAIL_PATTERN))) {
        sys_move_cursor(0, 2);
        printf("[largefile] tail mismatch: got %c%c%c%c\n", buf[0], buf[1], buf[2], buf[3]);
        sys_close(fd);
        return -1;
    }
    else {
        sys_move_cursor(0, 2);
        printf("[largefile] verified tail\n");
    }

    // Verify head content.
    if (sys_lseek(fd, 0, SEEK_SET) < 0) {
        sys_move_cursor(0, 3);
        printf("[largefile] lseek head failed\n");
        sys_close(fd);
        return -1;
    }
    memset(buf, 0, sizeof(buf));
    if (sys_fread(fd, buf, strlen(HEAD_PATTERN)) != (int)strlen(HEAD_PATTERN)) {
        sys_move_cursor(0, 3);
        printf("[largefile] read head failed\n");
        sys_close(fd);
        return -1;
    }
    if (!bytes_equal(buf, HEAD_PATTERN, (int)strlen(HEAD_PATTERN))) {
        sys_move_cursor(0, 3);
        printf("[largefile] head mismatch: got %c%c%c%c\n", buf[0], buf[1], buf[2], buf[3]);
        sys_close(fd);
        return -1;
    }
    else {
        sys_move_cursor(0, 3);
        printf("[largefile] verified head\n");
    }

    sys_close(fd);
    sys_move_cursor(0, 4);
    printf("[largefile] PASSED\n", LARGE_TARGET);
    return 0;
}
