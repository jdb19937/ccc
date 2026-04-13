#include \
<stdio.h>

int main\
(void)\
{
int valores[] = \
{1000,900,500,400,\
100,90,50,40,\
10,9,5,4,1};

const char *signa[] = \
{"M","CM","D","CD",\
"C","XC","L","XL",\
"X","IX","V","IV","I"};

int numeri[] = \
{1776,1999,2025,\
42,3999,404};

for (int k = 0;\
k < 6; k++)\
{
int n = \
numeri[k];
printf(\
"%4d = ", n);
for (int i = 0;\
i < 13; i++)\
while \
(n >= valores[i])\
{
printf("%s",\
signa[i]);
n -= \
valores[i];
}
printf(\
"\n");
}

return \
0;
}
