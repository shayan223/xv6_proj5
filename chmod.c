#ifdef CS333_P5
#include "types.h"
#include "user.h"
#include "param.h"

int
main(int argc, char* argv[])
{
  
  if(argc != 3)
  {
    printf(1, "\n%s\n$", "Invalid arguments");
    exit();
  }

  if(strlen(argv[1]) != 4)
  {
    printf(1, "\n%s\n$", "Invalid argument");
    exit();
  }

  int octal = atoo(argv[1]);
  if(octal == 0 && strcmp(argv[1], "0000") != 0)
  {
    printf(1, "\n%s\n$", "Invalid argument");
    exit();
  }

  printf(1, "%d\n", octal);
  printf(1, "%s\n", argv[2]);

  chmod(argv[2], octal);
  exit();
}


#endif
