#include <stdio.h>
int main(void)
{
int qd[3][3];
int nota[10] = {0};
                                                           int tot = 0;
for (qd[0][0] = 1; qd[0][0] <= 9; qd[0][0]++) {
nota[qd[0][0]] = 1;
  for (qd[0][1] = 1; qd[0][1] <= 9; qd[0][1]++) {
  if (nota[qd[0][1]]) continue;
  nota[qd[0][1]] = 1;
    qd[0][2] = 15 - qd[0][0] - qd[0][1];
    if (qd[0][2]<1||qd[0][2]>9||nota[qd[0][2]]) {nota[qd[0][1]]=0; continue;}
    nota[qd[0][2]] = 1;
      for (qd[1][0] = 1; qd[1][0] <= 9; qd[1][0]++) {
      if (nota[qd[1][0]]) continue;
      nota[qd[1][0]] = 1;
        for (qd[1][1] = 1; qd[1][1] <= 9; qd[1][1]++) {
        if (nota[qd[1][1]]) continue;
        nota[qd[1][1]] = 1;
          qd[1][2] = 15 - qd[1][0] - qd[1][1];
          if (qd[1][2]<1||qd[1][2]>9||nota[qd[1][2]]) {nota[qd[1][1]]=0; continue;}
          nota[qd[1][2]] = 1;
            qd[2][0] = 15 - qd[0][0] - qd[1][0];
            qd[2][1] = 15 - qd[0][1] - qd[1][1];
            qd[2][2] = 15 - qd[0][2] - qd[1][2];
            if (qd[2][0]>=1&&qd[2][0]<=9&&!nota[qd[2][0]]
              &&qd[2][1]>=1&&qd[2][1]<=9&&!nota[qd[2][1]]
              &&qd[2][2]>=1&&qd[2][2]<=9&&!nota[qd[2][2]]
              &&qd[2][0]+qd[2][1]+qd[2][2]==15
              &&qd[0][0]+qd[1][1]+qd[2][2]==15
              &&qd[0][2]+qd[1][1]+qd[2][0]==15) {
tot++;
printf("#%d\n", tot);
printf("  %d %d %d\n", qd[0][0],qd[0][1],qd[0][2]);
printf("  %d %d %d\n", qd[1][0],qd[1][1],qd[1][2]);
printf("  %d %d %d\n", qd[2][0],qd[2][1],qd[2][2]);
            }
          nota[qd[1][2]] = 0;
        nota[qd[1][1]] = 0;
        }
      nota[qd[1][0]] = 0;
      }
    nota[qd[0][2]] = 0;
  nota[qd[0][1]] = 0;
  }
nota[qd[0][0]] = 0;
}
printf("%d quadrata magica\n", tot);
return 0;
}
