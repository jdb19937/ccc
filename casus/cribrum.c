#include <stdio.h>

#define N 200

int p[N];

int main(void) {
                  for (int i = 0; i < N; i++)
                        p[i] = 1;
                  p[0] = p[1] = 0;
                  for (int i = 2; i * i < N; i++)
                        if (p[i])
                              for (int j = i * i; j < N; j += i)
                                    p[j] = 0;
                  int c = 0;
                  for (int i = 2; i < N; i++)
                        if (p[i]) {
                              printf("%d ", i);
                              c++;
                        }
                  printf("\n%d primi sub %d\n", c, N);
                  return 0;
}
