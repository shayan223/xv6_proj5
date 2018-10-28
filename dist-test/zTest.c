#ifdef CS333_P3
/*
This test was created by pdx-cs slack
user tango (pretty sure its chris T.)
who made this as a modified version
of Josh W.'s runTest.c test
*/
#include "types.h"
#include "user.h"
#include "uproc.h"


void
zombietest(void)
{
  int ret = 1;

  for(int i = 0; i < 10; i++)
  {
    if(ret > 0){
      printf(1, "Looping process created\n");
      ret = fork();
    }else{
      break;
    }
  }

  if(ret > 0){
    printf(1, "PRESS CTRL-Z and CTRL-P TO SHOW LISTS\n");
    printf(1, "-------------------------------------\n");
    sleep(7*TPS);
    while(wait() != -1);
  }

  if(ret > 0)
  kill(ret);
  //for(;;) ret = 2 + 1; // Keeps for a running process

}


int 
main(int argc,char *argv[])
{
  zombietest();
  exit();
}

#endif
