/* dicta.c — series audioactiva Conway */
#include <stdio.h>
#include <string.h>

#define LIM_CHORDA 256
#define LONGITUDO(s) ((int)strlen((s)))
#define AD_DIGITUM(n) ((char)((n) + '0'))
#define PONE(alv, pos, c) do{ (alv)[(pos)++] = (c); }while(0)
#define DUO_PONE(alv, pos, n, c) do{ PONE(alv, pos, AD_DIGITUM(n)); PONE(alv, pos, c); }while(0)

int main(void)
{
    char currens[LIM_CHORDA] = "1";
    char novum[LIM_CHORDA];

    for(int t = 0; t < 12; t++){
        printf("%2d: %s\n", t + 1, currens);
        int lon = LONGITUDO(currens);
        int pos = 0;
        int i = 0;
        while(i < lon){
            char c = currens[i];
            int num = 0;
            while(i < lon && currens[i] == c){ i++; num++; }
            DUO_PONE(novum, pos, num, c);
        }
        novum[pos] = '\0';
        if(t < 11) strcpy(currens, novum);
    }
    return(0);
}
