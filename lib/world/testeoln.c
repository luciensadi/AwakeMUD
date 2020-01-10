#include <stdio.h>
#include <stdlib.h>

int main (int argc, char *argv[]) {
  FILE *fl;
  char buf[256];
  int i, j;

  if (argc < 2) {
    printf("Usage: test <file1> <file2> ...\n");
    exit(1);
  }

  for (i = 1; i < argc; i++) {
    if (!(fl = fopen(argv[i], "r"))) {
      printf("Error opening '%s' -- skipping.\n", argv[i]);
      continue;
    }
    printf("%s: ", argv[i]);
    fgets(buf, 256, fl);
    for (j = 0; buf[j]; j++) {
      if (buf[j] == '\r')
        printf("\\r");
      else if (buf[j] == '\n')
        printf("\\n");
    }
    printf("\n");
    fclose(fl);
  }
  printf("Done.\n");
  exit(0);
}