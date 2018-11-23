#ifdef CS333_P5
#include "types.h"
#include "user.h"
#include "param.h"

int
main(int argc, char* argv[])
{
 
  if(argc != 3)
  {
    printf(1, "\n%s\n$", "not enough arguments");
    exit();
  }

  if(strlen(argv[1]) < 0)
  {
    printf(1, "\n%s\n$", "Invalid argument");
    exit();
  }

  int newGid = atoi(argv[1]);

  if(newGid < 0)
  {
    printf(1, "\n%s\n$", "Invalid argument");
    exit();
  }

  chgrp(argv[2], newGid);
  exit();

}

#endif
