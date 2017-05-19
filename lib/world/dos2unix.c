#include <stdio.h>

int main (int argc, char *argv[])
{
  FILE *fl;
  char buf[256];
  int i;

  if (argc < 2) {
    printf("Usage: dos2unix <file1> <file2> ...\n");
    exit(1);
  }

  for (i = 1; i < argc; i++) {
    if (!(fl = fopen(argv[i], "r"))) {
      printf("File '%s' does not exist, skipping.\n", argv[i]);
      continue;
    }
    fclose(fl);
    sprintf(buf, "tr -d '\015' < %s > %s.tmp", argv[i], argv[i]);
    system(buf);
    sprintf(buf, "mv %s.tmp %s", argv[i], argv[i]);
    system(buf);
    printf("'%s' converted successfully.\n", argv[i]);
  }
  printf("Done.\n");
  exit(0);
}