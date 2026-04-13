/* constans.c — constans Kaprekar 6174 */
#include <stdio.h>

void digiti(int n,int d[4])
{
    for(int i=3;i>=0;i--,n/=10)d[i]=n%10;
}

void ordina(int d[4],int asc)
{
    for(int i=0;i<3;i++)
        for(int j=i+1;j<4;j++)
            if(asc?d[i]>d[j]:d[i]<d[j]){int t=d[i];d[i]=d[j];d[j]=t;}
}

int collige(int d[4])
{
    return d[0]*1000+d[1]*100+d[2]*10+d[3];
}

int main(void)
{
    int numeri[]={3087,7641,1234,9998,1000};
    for(int k=0;k<5;k++){
        int n=numeri[k];
        printf("%04d:",n);
        for(int g=0;g<8&&n!=6174;g++){
            int a[4],d[4];
            digiti(n,a);digiti(n,d);
            ordina(a,1);ordina(d,0);
            n=collige(d)-collige(a);
            printf(" %04d",n);
        }
        printf("\n");
    }
    return 0;
}
