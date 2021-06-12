#include <stdio.h>
FILE *f;
int main()
{
  char TmpBuf[10];
  f = fopen("send.txt", "r");
  while (!feof(f))
  {
     unsigned int n = fread(TmpBuf, sizeof(char), (sizeof(TmpBuf)-1), f);
     if (n == 0)
       break;
     TmpBuf[n] = '\0';
     puts(TmpBuf);
  }
  fflush(f);
  fclose(f);
  return 0;
}
