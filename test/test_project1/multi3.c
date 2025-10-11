#include <kernel.h>

int strlen(const char *src)
{
    int i = 0;
    while (src[i] != '\0') {
        i++;
    }
    return i;
}

void strrev(char *s) {
    int s_len = strlen(s);
    for(int i = 0; i < s_len / 2; i++) {
        char tmp;
        tmp = s[i];
        s[i] = s[s_len - i - 1];
        s[s_len - i - 1] = tmp;
    }
}

void itoa(int n, char s[]) {
    int i = 0, sign;
    
    if ((sign = n) < 0) {
        n = -n;
    }
    
    do {
        s[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);
    
    if (sign < 0) {
        s[i++] = '-';
    }
    
    s[i] = '\0';
    
    strrev(s);
}

int main(int num) {
    int result = num * 3;
    char s[32];
    itoa(result, s);
    bios_putstr("[multi3] multi3.c: ");
    bios_putstr(s);
    bios_putstr("\n\r");
    return result;
}