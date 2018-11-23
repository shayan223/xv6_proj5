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
  if(strlen(argv[2]) < 0)
  {
    printf(1, "\n%s\n$", "Invalid argument");
    exit();
  }
  int newUid = atoi(argv[1]);

  chown(argv[2], newUid);

  exit();

}


#endif
