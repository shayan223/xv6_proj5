#ifdef CS333_P2
#include "types.h"
#include "user.h"
#include "param.h"


int
main(int argc, char* argv[])
{
  int startTime = uptime();
  int runTime = 0;

  if(fork() == 0)
  {
    argv++; 
    
    exec(argv[0], argv);
    exit();
  }
  else
    wait();
  
  runTime = uptime() - startTime;
  
    printf(1, "\n%s%s%s", "Time taken to execute ", argv[1], ": ");
    int sec = (runTime/1000);
    int remainder = (runTime % 1000);
    printf(1, "%d%s", sec, ".");
    if(remainder<100)
      printf(1, "%d",0);
    if(remainder<10) 
      printf(1, "%d",0);
    printf(1, "%d\n", remainder);


  exit();
}

#endif




