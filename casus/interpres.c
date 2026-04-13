/* interpres.c — interpres linguae BF */
#include <stdio.h>
#include <string.h>
int age(const char *prog)
{
    unsigned char mem[256] = {0};
    int ptr = 0, pos = 0, lon = (int)strlen(prog);
    while (pos < lon) {
        switch (prog[pos]) {
        case '>': ptr++; break;
        case '<': ptr--; break;
        case '+': mem[ptr]++; break;
        case '-': mem[ptr]--; break;
        case '.': putchar(mem[ptr]); break;
        case '[': if (!mem[ptr]) { int d = 1; while (d) { pos++; if (prog[pos] == '[') d++; else if (prog[pos] == ']') d--; } } break;
        case ']': if (mem[ptr]) { int d = 1; while (d) { pos--; if (prog[pos] == ']') d++; else if (prog[pos] == '[') d--; } } break;
        default: break;
        }
        pos++;
    }
    return 0;
}
int main(void)
{
    printf("numeri: ");
    age("++++++[>++++++++<-]>.+.+.+.+.+.+.+.+.+.");
    printf("\n");
    printf("salve:  ");
    age("++++++++[>++++++++++>++++++++>+++++++++>++++++++++>++++++++<<<<<-]>+++.>+.>++++.>++++++.>+++++.");
    printf("\n");
    return 0;
}
