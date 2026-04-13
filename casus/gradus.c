/* gradus.c -- gradus ad 1 */
#include <stdio.h>
int main(void){int n=937,g=0;while(n!=1){
if(n%2==0)n/=2;else n=3*n+1;g++;}
printf("937 -> 1: %d gradus\n",g);return 0;}
