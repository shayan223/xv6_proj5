#ifdef CS333_P2
#include "types.h"
#include "user.h"
#include "param.h"
#include "uproc.h"


int
main()
{
  struct uproc * newTable;
  newTable = (struct uproc*) malloc(psMAX * sizeof(struct uproc));
  int spacesUsed = getprocs(psMAX, newTable);
  
  if(spacesUsed < 0)
    exit();
  if(spacesUsed > psMAX)
    exit();
  //use newTable to display

  printf(1, "\n|%s\t|%s\t|%s\t|%s\t|%s\t|%s\t|%s\t|%s\t|%s\t", "PID", "Name", "UID", "GID", "PPID", "Elapsed", "CPU", "State", "Size");

  for(int i = 0; i < spacesUsed; ++i)
  {
    printf(1, "\n|%d\t|%s\t|%d\t|%d\t|%d\t", newTable[i].pid, newTable[i].name, newTable[i].uid, newTable[i].gid, newTable[i].ppid);

    int sec = (newTable[i].elapsed_ticks/1000);
    int remainder = (newTable[i].elapsed_ticks % 1000);
    printf(1, "|%d%s", sec, ".");
    if(remainder<100)
      printf(1, "%d",0);
    if(remainder<10) 
      printf(1, "%d",0);
    printf(1, "%d\t", remainder);

    sec = (newTable[i].CPU_total_ticks/1000);
    remainder = (newTable[i].CPU_total_ticks % 1000);
    printf(1, "\t|%d%s", sec, ".");
    if(remainder<100)
      printf(1, "%d",0);
    if(remainder<10) 
      printf(1, "%d",0);
    printf(1, "%d\t", remainder);

    printf(1, "|%s\t", newTable[i].state);

    printf(1, "|%d", newTable[i].size);


  }
//----------------------------------------------
  printf(1, "\n%s %d\n", "Proc table spaces used: ",spacesUsed);
  //free memory
  free(newTable); 
  exit();
}




#endif
