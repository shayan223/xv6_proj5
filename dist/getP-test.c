/********
This test was provided by Christopher Teters
All credit and many thanks for this test program 
go to him.
********/
#ifdef CS333_P4
#include "types.h"
#include "user.h"
#include "param.h"

void
testgetprio(void)
{
printf(1, "The PID of the currently running process is found to be: %d\n", getpriority(getpid()));
printf(1, "The PID for the first running shell of the program is found to be: %d\n", getpriority(2));

printf(1, "Checking for a nonexisting PID of 999\n");
if(getpriority(999) < 0){
printf(1, "Returned a negative value, indicating that nothing was changed\n");
printf(1, "This test passed\n");
}
else
printf(1, "This test failed\n");
}

int
main(int argc, char *argv[])
{
printf(1, "\nTesting getpriority()\n");
printf(1, "Current settings: MAXPRIO = %d\n", MAXPRIO);

testgetprio();

exit();
}
#endif
