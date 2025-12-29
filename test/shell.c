/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                  The shell acts as a task running in user mode.
 *       The main function is to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define SHELL_BEGIN 20
#define MAX_CH 1024
#define MAX_COMMANDS 96

char buf[MAX_CH];
int buf_sp = 0;
char *commands[MAX_COMMANDS] = {"ps", "clear", "exec", "kill", "exit", "waitpid", "taskset"};

int try_parse_digit(char *str, char *command) {
    int len = strlen(str);
    
    int parse_failed = 0;
    if(len > 2 && str[0] == '0' && (str[1] == 'X' || str[1] == 'x')) {
        for(int j = 2; j < len; j++) {
            if(!isxdigit(str[j])) {
                parse_failed = 1;
            }
        }
    } else {
        for(int j = 0; j < len; j++) {
            if(!isdigit(str[j])) {
                parse_failed = 1;
            }
        }
    }
    return parse_failed;
}

int main(void)
{
    sys_clear();
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");

    while (1)
    {
        // TODO [P3-task1]: call syscall to read UART port
        printf("> root@UCAS_OS: ");
        memset(buf, 0, sizeof(buf));
        buf_sp = 0;        
        int ch;
        while((ch = sys_getchar()) != '\n' && ch != '\r') {
            if(ch == -1) {
                continue;
            }
            else if(ch == '\b' || ch == 127) {
                if(buf_sp > 0) {
                    buf_sp--;
                    buf[buf_sp] = '\0';
                    printf("\b");
                }
                continue;
            }
            if(buf_sp >= MAX_CH) {
                printf("Command is too loog\n");
                continue;
            }
            buf[buf_sp++] = ch;
            printf("%c", ch);
        }
        printf("\n");
        
        // TODO [P3-task1]: parse input
        // note: backspace maybe 8('\b') or 127(delete)
        while(buf[buf_sp-1] == ' ') {
            buf[buf_sp-1] = '\0';
            buf_sp--;
        }
        int startpos;
        for(startpos = 0; startpos < buf_sp && buf[startpos] == ' '; startpos++);
        int in_word = 0;
        int argc = 0;
        char *argv[1024];
        
        for(int i = startpos; i < buf_sp; i++) {
            if(buf[i] == ' ') {
                in_word = 0;
                buf[i] = '\0';
            }
            else {
                if(in_word == 0) {
                    argv[argc++] = buf + i;
                }
                in_word = 1;
            }
        }

        if(argc == 0) continue;

        //"ps", "clear", "exec", "kill", "exit", "waitpid"
        // TODO [P3-task1]: ps, exec, kill, clear    
        if(strcmp(argv[0], "ps") == 0) {
            sys_ps();
        } else if(strcmp(argv[0], "clear") == 0) {
            sys_clear();
            sys_move_cursor(0, SHELL_BEGIN);
            printf("------------------- COMMAND -------------------\n");
        } else if(strcmp(argv[0], "exec") == 0) {
            if(argc <= 1) {
                printf("exec: too few arguments\n");
            } else {
                pid_t pid = sys_exec(argv[1], argc, argv);
                if(pid == 0) {
                    printf("ERROR: task not found\n");
                    continue;
                } else {
                    printf("Info: execute %s successfully, pid = %d ...\n", argv[1], pid);
                }
                if(argc >= 3 && strcmp(argv[argc - 1], "&") == 0) {
                    continue;
                }
                sys_waitpid(pid);
            }
        } else if(strcmp(argv[0], "kill") == 0) {
            if(argc <= 1) {
                printf("kill: too few arguments\n");
            } else {
                if(try_parse_digit(argv[1], argv[0])) {
                    printf("kill: illegal argument: not a digit\n");
                    continue;
                } else {
                    int success = sys_kill(atoi(argv[1]));
                    if(!success) {
                        printf("ERROR: PID not found\n");
                    }
                }
            }
        } else if(strcmp(argv[0], "exit") == 0) {
            sys_exit();
        } else if(strcmp(argv[0], "waitpid") == 0) {
            if(argc <= 1) {
                printf("waitpid: too few arguments\n");
            } else {
                if(try_parse_digit(argv[1], argv[0])) {
                    continue;
                } else {
                    int success = sys_waitpid(atoi(argv[1]));
                    if(!success) {
                        printf("ERROR: PID not found\n");
                    }
                }
            }
        } else if(strcmp(argv[0], "taskset") == 0) {
            if(argc <= 1) {
                printf("taskset: too few arguments\n");
                continue;
            }
            if(strcmp(argv[1], "-p") == 0) {
                if(argc < 4) {
                    printf("taskset: too few arguments\n");
                    continue;
                }
                uint32_t mask = atoi(argv[2]);
                pid_t pid = atoi(argv[3]);
                int success = sys_taskset_p(mask, pid);
                if(!success) {
                    printf("ERROR: PID not found\n");
                }
            } else {
                if(argc < 3) {
                    printf("taskset: too few arguments\n");
                    continue;
                }
                uint32_t mask = atoi(argv[1]);
                pid_t pid = sys_taskset(mask, argv[2]);
                if(pid == 0) {
                    printf("ERROR: task not found\n");
                    continue;
                }
            }
        } else if(strcmp(argv[0], "free") == 0) {
            if(argc >= 3) {
                printf("free: too many arguments\n");
                continue;
            }
            if(argc == 1) {
                size_t free_mem = sys_free_mem();
                printf("Free memory: %ld B\n", free_mem);
            } else {    // argc == 2
                if(strcmp(argv[1], "-h") == 0) {
                    size_t free_mem = sys_free_mem();
                    if(free_mem >= 1048576) {   // 1MB
                        free_mem /= 1048576;
                        printf("Free memory: %ld MB\n", free_mem);
                    } else if (free_mem >= 1024) {  // 1KB
                        free_mem /= 1024;
                        printf("Free memory: %ld KB\n", free_mem);
                    } else {
                        printf("Free memory: %ld B\n", free_mem);
                    }
                }
            }
        } else {
            printf("%s: command not found\n", argv[0]);
        }

        /************************************************************/
        // TODO [P6-task1]: mkfs, statfs, cd, mkdir, rmdir, ls

        // TODO [P6-task2]: touch, cat, ln, ls -l, rm
        /************************************************************/
    }

    return 0;
}
