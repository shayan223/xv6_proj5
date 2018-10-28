#ifdef CS333_P3
/*
Test written and supplied
by Josh W.
*/

#include "types.h"
#include "user.h"
#include "uproc.h"


int 
main(int argc,char *argv[]){

  int ret = 1;
  
  for(int i = 0; i < 5; i++)
  {
    if(ret > 0){
      printf(1, "Looping process created\n");
      ret = fork();
    }else{
      break;
    }
  }  

  if(ret > 0){ 
    printf(1, "PRESS CTRL-R and CTRL-P TO SHOW LISTS\n");
    printf(1, "-------------------------------------\n");
    sleep(7*TPS);
    while(wait() != -1);
  }

  for(;;){
    ret = 1 + 1;
  }

  exit();

}

#endif
