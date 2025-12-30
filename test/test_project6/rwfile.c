#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char buff[64];

int main(void)
{
    int fd = sys_open("1.txt", O_RDWR);

    // write 'hello world!' * 10
    for (int i = 0; i < 10; i++)
    {
        sys_fwrite(fd, "hello world!\n", 13);
    }

    // rewind to the start before reading back
    sys_lseek(fd, 0, SEEK_SET);

    // read
    for (int i = 0; i < 10; i++)
    {
        sys_fread(fd, buff, 13);
        for (int j = 0; j < 13; j++)
        {
            printf("%c", buff[j]);
        }
    }

    sys_close(fd);

    return 0;
}