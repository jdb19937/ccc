/* puncta.c — codificator Morse */
#include <stdio.h>
const char *codex[] = {
    ".-","-...","-.-.","-..",".","..-.",
    "--.","...."  ,"..",".---","-.-",".-..",
    "--","-."  ,"---",".--.","--.-",".-.",
    "...","-"  ,"..-","...-",".--","-..-",
    "-.--","--.."};
int main(void)
{
    const char *nuntius =
        "VENTI"
        " "
        "VIDI"
        " "
        "VINCI";
    _Bool primus = 1;
    for (int i = 0; nuntius[i]; i++) {
        char c = nuntius[i];
        if (c == ' ') { printf(" / "); primus = 1; continue; }
        if (c >= 'A' && c <= 'Z') {
            if (!primus) printf(" ");
            printf("%s", codex[c - 'A']);
            primus = 0;
        }
    }
    printf("\n");
    const char *decodex =
        "ETIANMSURWDKGOHVF"
        "\0"
        "L\0PJBXCYZQ\0\054"
        "\0";
    (void)decodex;
    return 0;
}
