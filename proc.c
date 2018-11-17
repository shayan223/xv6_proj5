#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#ifdef CS333_P2
#include "uproc.h"
#endif
static char *states[] = {
[UNUSED]    "unused",
[EMBRYO]    "embryo",
[SLEEPING]  "sleep ",
[RUNNABLE]  "runble",
[RUNNING]   "run   ",
[ZOMBIE]    "zombie"
};

#ifdef CS333_P3
struct ptrs {
  struct proc * head;
  struct proc * tail;
};
#endif



static struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  #ifdef CS333_P3
  struct ptrs list[statecount];
  #endif
  #ifdef CS333_P4
  struct ptrs ready[MAXPRIO+1];
  uint PromoteAtTime;
  #endif
} ptable;

static struct proc *initproc;

uint nextpid = 1;
extern void forkret(void);
extern void trapret(void);
static void wakeup1(void* chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;

  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid) {
      return &cpus[i];
    }
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//-------------------------------------------//
#ifdef CS333_P3
static void
stateListAdd(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL){
    (*list).head = p;
    (*list).tail = p;
    p -> next = NULL;
  } else {
    ((*list).tail) -> next = p;
    (*list).tail = ((*list).tail) -> next;
    ((*list).tail) -> next = NULL;
  }
}


static int
stateListRemove(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL || (*list).tail == NULL || p == NULL)
  {
    return -1;
  }

  struct proc* current = (*list).head;
  struct proc* previous = 0;

  if(current == p)
  {
    (*list).head = ((*list).head) -> next;
    //prevent tail remaining assigned when we've removed the only item
    //on the list
    if((*list).tail == p)
    {
      (*list).tail = NULL;
    }
    return 0;
  }

  while(current){
    if(current == p)
    {
      break;
    }

    previous = current;
    current = current->next;
  }

  // Process not found, hit eject.
  if(current == NULL)
  {
    return -1;
  }
  // Process found. Set the appropriate next pointer.
  if(current == (*list).tail)
  {
    (*list).tail = previous;
    ((*list).tail)->next = NULL;
  } else {
      previous->next = current->next;
    }
  // Make sure p->next doesn’t point into the list.
  p->next = NULL;
  return 0;
}


static void
initProcessLists()
{
  int i;
  for (i = UNUSED; i <= ZOMBIE; i++) 
  {
    ptable.list[i].head = NULL;
    ptable.list[i].tail = NULL;

  }

  #ifdef CS333_P4
  for (i = 0; i <= MAXPRIO; i++) 
  {
  ptable.ready[i].head = NULL;
  ptable.ready[i].tail = NULL;
  }
  #endif

}


static void
initFreeList(void)
{
  struct proc* p;

  for(p = ptable.proc; p < ptable.proc + NPROC; ++p){
    p->state = UNUSED;
    stateListAdd(&ptable.list[UNUSED], p);
  }
}


//Assert function added//

static int
assertState(struct proc *p, enum procstate state)
{
  if(!p)
  {
    panic("invalid proc pointer");
    return -1;
  }

  else if(p -> state != state)
  {
    return -1;
  }
  else{
    return 0;
  }
}

#endif
//-------------------------------------------//


//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
#ifdef CS333_P3
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  p = ptable.list[UNUSED].head;
  if(!p)
  {
    release(&ptable.lock);
    return 0;
  }
  //---Added Code----//
  stateListRemove(&ptable.list[UNUSED], p);
  if(assertState(p, UNUSED) < 0)
  {
    panic("new proc not taken from UNUSED");
  }
  p->state = EMBRYO;
  p->pid = nextpid++;

  #ifdef CS333_P4
  p -> budget = BUDGET;
  p -> priority = MAXPRIO;//TODO
  #endif

  stateListAdd(&ptable.list[EMBRYO], p);
  //-----------------//
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  #ifdef CS333_P1
  p->start_ticks = ticks;
  #endif

  #ifdef CS333_P2
  p->cpu_ticks_total = 0;
  p->cpu_ticks_in = 0;
  #endif
/*
  #ifdef CS333_P4
  p -> budget = BUDGET;
  p -> priority = MAXPRIO;//TODO
  #endif
  */

  return p;
}

//------------------------------

#else

static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  int found = 0;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED) {
      found = 1;
      break;
    }
  if (!found) {
    release(&ptable.lock);
    return 0;
  }
  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);
  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  #ifdef CS333_P1
  p->start_ticks = ticks;
  #endif

  #ifdef CS333_P2
  p->cpu_ticks_total = 0;
  p->cpu_ticks_in = 0;
  #endif
  return p;
}

#endif


//PAGEBREAK: 32
// Set up first user process.
#ifdef CS333_P4
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  //-----Added code-----
  acquire(&ptable.lock);
  initProcessLists();
  initFreeList();
  release(&ptable.lock);
  //---------------------
  p = allocproc();

  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S
  //---more initializations---//
  //p -> PromoteAtTime = ticks + TICKS_TO_PROMOTE;
  ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
  p -> budget = BUDGET;
  //-------------------------//
  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);
  //----Added Code ----//
  stateListRemove(&ptable.list[EMBRYO], p);
  if(assertState(p, EMBRYO) < 0)
  {
    panic("line 359: removed Proc not EMBRYO");
  }
  p->state = RUNNABLE;
  //p->priority = MAXPRIO;//TODO
  stateListAdd(&ptable.ready[p -> priority], p);
  //-------------------//
  release(&ptable.lock);
}


#elif CS333_P3
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  //-----Added code-----
  acquire(&ptable.lock);
  initProcessLists();
  initFreeList();
  release(&ptable.lock);
  //---------------------
  p = allocproc();

  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);
  //----Added Code ----//
  stateListRemove(&ptable.list[EMBRYO], p);
  if(assertState(p, EMBRYO) < 0)
  {
    panic("line 359: removed Proc not EMBRYO");
  }
  p->state = RUNNABLE;
  stateListAdd(&ptable.list[RUNNABLE], p);
  //-------------------//
  release(&ptable.lock);
}

#else
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);
  #ifdef CS333_P2
  p -> uid = UID;
  p -> gid = GID;
  #endif
  p->state = RUNNABLE;
  release(&ptable.lock);
}
#endif

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}
#ifdef CS333_P4
// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i;
  uint pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;
  #ifdef CS333_P2
  np -> uid = curproc -> uid;
  np -> gid = curproc -> gid;
  #endif
  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;
  
  //---Added Code---//
  acquire(&ptable.lock);
  stateListRemove(&ptable.list[np->state], np);
  if(assertState(np, EMBRYO) < 0)
  {
    panic("forked proc not from EMBRYO");
  }
  np->state = RUNNABLE;
  //np->priority = MAXPRIO;
  stateListAdd(&ptable.ready[np->priority], np);
  release(&ptable.lock);
  //----------------//

  return pid;
}
//----------------------------


#elif CS333_P3
// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i;
  uint pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;
  #ifdef CS333_P2
  np -> uid = curproc -> uid;
  np -> gid = curproc -> gid;
  #endif
  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;
  
  //---Added Code---//
  acquire(&ptable.lock);
  stateListRemove(&ptable.list[np->state], np);
  if(assertState(np, EMBRYO) < 0)
  {
    panic("forked proc not from EMBRYO");
  }
  np->state = RUNNABLE;
  stateListAdd(&ptable.list[np->state], np);
  release(&ptable.lock);
  //----------------//

  return pid;
}
//----------------------------
#else
// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i;
  uint pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;
  #ifdef CS333_P2
  np -> uid = curproc -> uid;
  np -> gid = curproc -> gid;
  #endif
  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);

  return pid;
}

#endif

#ifdef CS333_P4
// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  p = ptable.list[ZOMBIE].head;
  while(p)
  {
    if(p -> parent == curproc)
    {
      p -> parent = initproc;
      wakeup1(initproc);
    }
    p = p -> next;
  }
  
  //p = ptable.list[RUNNABLE].head;

  for(int i = MAXPRIO; i >= 0; --i)//TODO 
  {
    p = ptable.ready[i].head;
    while(p)
    {
      if(p -> parent == curproc)
      {
        p -> parent = initproc;
      }  
      p = p -> next; 
    }
  }
  /*while(p)
  {
    if(p->parent == curproc)
    {
      p -> parent = initproc;
    }
    p = p -> next;
  }*/
  
  p = ptable.list[RUNNING].head;
  while(p)
  {
    if(p->parent == curproc)
    {
      p -> parent = initproc;
    }
    p = p -> next;


  }
  
  p = ptable.list[SLEEPING].head;
  while(p)
  {
    if(p->parent == curproc)
    {
      p -> parent = initproc;
    }
    p = p -> next;


  }
  
  p = ptable.list[EMBRYO].head;
  while(p)
  {
    if(p->parent == curproc)
    {
      p -> parent = initproc;
    }
    p = p -> next;


  }

  //---------------------------*/

  // Jump into the scheduler, never to return.
  //----Added Code----//
  stateListRemove(&ptable.list[RUNNING], curproc);
  if(assertState(curproc, RUNNING) < 0)
  {
    panic("proc moved to zombie not from RUNNING");
  }
  curproc->state = ZOMBIE;
  stateListAdd(&ptable.list[curproc -> state], curproc);
  sched();
  panic("zombie exit");
}

//-------------------------------------------------


#elif CS333_P3
// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  p = ptable.list[ZOMBIE].head;
  while(p)
  {
    if(p -> parent == curproc)
    {
      p -> parent = initproc;
      wakeup1(initproc);
    }
    p = p -> next;
  }
  
  p = ptable.list[RUNNABLE].head;
  while(p)
  {
    if(p->parent == curproc)
    {
      p -> parent = initproc;
    }
    p = p -> next;
  }
  
  p = ptable.list[RUNNING].head;
  while(p)
  {
    if(p->parent == curproc)
    {
      p -> parent = initproc;
    }
    p = p -> next;


  }
  
  p = ptable.list[SLEEPING].head;
  while(p)
  {
    if(p->parent == curproc)
    {
      p -> parent = initproc;
    }
    p = p -> next;


  }
  
  p = ptable.list[EMBRYO].head;
  while(p)
  {
    if(p->parent == curproc)
    {
      p -> parent = initproc;
    }
    p = p -> next;


  }

  //---------------------------*/

  // Jump into the scheduler, never to return.
  //----Added Code----//
  stateListRemove(&ptable.list[RUNNING], curproc);
  if(assertState(curproc, RUNNING) < 0)
  {
    panic("proc moved to zombie not from RUNNING");
  }
  curproc->state = ZOMBIE;
  stateListAdd(&ptable.list[curproc -> state], curproc);
  sched();
  panic("zombie exit");
}

//-------------------------------------------------

#else
// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}
#endif

#ifdef CS333_P4
// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    
    //-----Added Code-----//
    p = ptable.list[ZOMBIE].head;
    while(p)
    {
      if(p -> parent == curproc)
      {
        havekids = 1;

        stateListRemove(&ptable.list[p->state],p);
        if(assertState(p, ZOMBIE) < 0)
        {
          panic("Proc moved to unused not zombie");
        }
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        stateListAdd(&ptable.list[p->state],p);
        release(&ptable.lock);
        return pid;
      }

    p = p -> next;
    }

    p = ptable.list[RUNNING].head;
    while(p)
    {
      if(p -> parent == curproc)
        havekids = 1;

      p = p -> next;
    }

    p = ptable.list[SLEEPING].head;
    while(p)
    {
      if(p -> parent == curproc)
        havekids = 1;

      p = p -> next;
    }

    struct proc * current;
    for(int i = MAXPRIO; i >= 0; --i)//TODO
    {
      current = ptable.ready[i].head;
      while(current)
      {
        p = current;
        current = current -> next;
          
        if(p -> parent == curproc)
        {
          havekids = 1;
        }
      }
    }



    p = ptable.list[EMBRYO].head;
    while(p)
    {
      if(p -> parent == curproc)
        havekids = 1;

      p = p -> next;
    }
    //-------------------//
    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

#elif CS333_P3
// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    
    //-----Added Code-----//
    p = ptable.list[ZOMBIE].head;
    while(p)
    {
      if(p -> parent == curproc)
      {
        havekids = 1;

        stateListRemove(&ptable.list[p->state],p);
        if(assertState(p, ZOMBIE) < 0)
        {
          panic("Proc moved to unused not zombie");
        }
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        stateListAdd(&ptable.list[p->state],p);
        release(&ptable.lock);
        return pid;
      }

    p = p -> next;
    }

    p = ptable.list[RUNNING].head;
    while(p)
    {
      if(p -> parent == curproc)
        havekids = 1;

      p = p -> next;
    }

    p = ptable.list[SLEEPING].head;
    while(p)
    {
      if(p -> parent == curproc)
        havekids = 1;

      p = p -> next;
    }

    p = ptable.list[RUNNABLE].head;
    while(p)
    {
      if(p -> parent == curproc)
        havekids = 1;

      p = p -> next;
    }

    p = ptable.list[EMBRYO].head;
    while(p)
    {
      if(p -> parent == curproc)
        havekids = 1;

      p = p -> next;
    }
    //-------------------//
    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
//----------------------------------------
#else
// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
#endif

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.

#ifdef CS333_P4
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6
    acquire(&ptable.lock);
    //check for promotion time and increase all priority
    //of all active processes if time has expired
    if(ticks > ptable.PromoteAtTime)
    {
      struct proc *p;
      struct proc *current = ptable.list[SLEEPING].head;

      while(current)
      {
        p = current;
        current = current -> next;

        if(p -> priority < MAXPRIO)
          p -> priority += 1;
      }

      
      for(int i = MAXPRIO; i >= 0; --i)
      {
        current = ptable.ready[i].head;
        while(current)
        {
          p = current;
          current = current -> next;
          
          if(p -> priority < MAXPRIO)
          {
            p -> priority += 1;

            stateListRemove(&ptable.ready[p->priority-1], p);
            stateListAdd(&ptable.ready[p->priority], p);
          }
        }
      }

      current = ptable.list[RUNNING].head;
      while(current)
      {
        p = current;
        current = current -> next;

        if(p -> priority < MAXPRIO)
          p -> priority += 1;
      }

      ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
    }
    
    // Check ready list looking for process to run.
        p = NULL;
        for(int i = MAXPRIO; i >= 0; --i)
        {
          if(ptable.ready[i].head)
          {
            p = ptable.ready[i].head;
            break;
          }
        }
         
      //if(!p)
       //cprintf("%s", "P not found");
      if(p){
        

        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
#ifdef PDX_XV6
        idle = 0;  // not idle this timeslice
#endif // PDX_XV6
        c->proc = p;
        switchuvm(p);
        //---Added code----//
        stateListRemove(&ptable.ready[p->priority], p);
        if(assertState(p, RUNNABLE) < 0)
        {
          panic("Line 640: new proc not taken from RUNNABLE");
        }
        p->state = RUNNING;
        stateListAdd(&ptable.list[p -> state], p); 
        //----------------//
#ifdef CS333_P2
        p->cpu_ticks_in = ticks;
#endif
        swtch(&(c->scheduler), p->context);
        switchkvm();

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        //p = p -> next;//move to next runnable proccess TODO
     // }
      }
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}

//------------------------------------------------//

#elif CS333_P3
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
      p = ptable.list[RUNNABLE].head;
      while(p){

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
#ifdef PDX_XV6
      idle = 0;  // not idle this timeslice
#endif // PDX_XV6
      c->proc = p;
      switchuvm(p);
      //---Added code----//
      stateListRemove(&ptable.list[RUNNABLE], p);
      if(assertState(p, RUNNABLE) < 0)
      {
        panic("Line 640: new proc not taken from RUNNABLE");
      }
      p->state = RUNNING;
      stateListAdd(&ptable.list[RUNNING], p); 
      //----------------//
#ifdef CS333_P2
      p->cpu_ticks_in = ticks;
#endif
      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      p = p -> next;//move to next runnable proccess
    }
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}

//------------------------------------------------//

#else
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
#ifdef PDX_XV6
      idle = 0;  // not idle this timeslice
#endif // PDX_XV6
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
#ifdef CS333_P2
      p->cpu_ticks_in = ticks;
#endif
      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}
#endif

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  #ifdef CS333_P2
  p -> cpu_ticks_total += ticks - p -> cpu_ticks_in;
  #endif
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;

}

#ifdef CS333_P4
// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock
  //---Added Code----//
  stateListRemove(&ptable.list[RUNNING], curproc);
  if(assertState(curproc, RUNNING) < 0)
  {
    panic("Proc Yielded not from RUNNING list");
  }
  curproc->state = RUNNABLE;
  curproc->budget -= ticks - curproc->cpu_ticks_in;
  if(curproc->budget <= 0)
  {
    curproc->budget = BUDGET;
    if(curproc -> priority > 0)
      curproc->priority -= 1;
  }
  stateListAdd(&ptable.ready[curproc->priority], curproc);
  sched();
  //---------//
  release(&ptable.lock);
}



#elif CS333_P3
// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock
  //---Added Code----//
  stateListRemove(&ptable.list[RUNNING], curproc);
  if(assertState(curproc, RUNNING) < 0)
  {
    panic("Proc Yielded not from RUNNING list");
  }
  curproc->state = RUNNABLE;
  stateListAdd(&ptable.list[curproc->state], curproc);
  sched();
  //---------//
  release(&ptable.lock);
}

#else
// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock
  curproc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

#endif

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
#ifdef CS333_P3
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  //----Added Code---//
  stateListRemove(&ptable.list[RUNNING], p);
  if(assertState(p, RUNNING) < 0)
  {
    panic("line 812: sleep proc not taken from RUNNING");
  }
  #ifdef CS333_P4

  p->budget -= ticks - p->cpu_ticks_in;
  if(p->budget <= 0)
  {
    p->budget = BUDGET;
    if(p-> priority > 0)
      p->priority -= 1;
  }

  #endif
  p->state = SLEEPING;
  stateListAdd(&ptable.list[SLEEPING], p);
  //release(&ptable.lock);
  //----------------//
  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}

//----------------------------------------//
#else
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}

#endif

//--------------------------------------------//
//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
#ifdef CS333_P4
static void
wakeup1(void *chan)
{
  struct proc *p;
  struct proc *current = ptable.list[SLEEPING].head;
  while(current)
  {
    p = current;
    current = current -> next;
    if(p -> chan == chan)
    {
      //---Code Added---//
      stateListRemove(&ptable.list[SLEEPING], p);
      if(assertState(p, SLEEPING) < 0)
      {
        panic("proc woken to runable not from SLEEPING");
      }
      p->state = RUNNABLE;
      stateListAdd(&ptable.ready[p -> priority], p);
      //---------------//
    }
    
  }

}

#elif CS333_P3
static void
wakeup1(void *chan)
{
  struct proc *p;
  struct proc *current = ptable.list[SLEEPING].head;
  while(current)
  {
    p = current;
    current = current -> next;
      //---Code Added---//
      stateListRemove(&ptable.list[SLEEPING], p);
      if(assertState(p, SLEEPING) < 0)
      {
        panic("proc woken to runable not from SLEEPING");
      }
      p->state = RUNNABLE;
      stateListAdd(&ptable.list[RUNNABLE], p);
      //---------------//
    
  }

}

#else

static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

#endif

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

#ifdef CS333_P4
// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;
  acquire(&ptable.lock);
//--------Added Code-------//
  p = ptable.list[EMBRYO].head;
  while(p)
  {
    if(p->pid == pid)
    {
      p -> killed = 1;
      release(&ptable.lock);
      return 0;
    }
    p = p -> next; 
  }
  p = ptable.list[SLEEPING].head;
  while(p)
  {
    if(p -> pid == pid)
    {
      p -> killed = 1;
      stateListRemove(&ptable.list[SLEEPING], p);
      if(assertState(p, SLEEPING) < 0)
      {
        panic("non-SLEEPING killed process moved to RUNNING");
      }
      p -> state = RUNNABLE;
      stateListAdd(&ptable.ready[p->priority], p);
      release(&ptable.lock);
      return 0;
    }
    p = p -> next;
  }

  for(int i = MAXPRIO; i >= 0; --i)//TODO 
  {
    p = ptable.ready[i].head;
    while(p)
    {
      if(p -> pid == pid)
      {
        p -> killed = 1;
        release(&ptable.lock);
        return 0;
      }  
      p = p -> next; 
    }
   }
 
/*
  p = ptable.list[RUNNABLE].head;
  while(p)
  {
    if(p->pid == pid)
    {
      p -> killed = 1;
      release(&ptable.lock);
      return 0;
    }
    p = p -> next; 
  }
*/
  p = ptable.list[RUNNING].head;
  while(p)
  {
    if(p->pid == pid)
    {
      p -> killed = 1;
      release(&ptable.lock);
      return 0;
    }
    p = p -> next; 
  }
  p = ptable.list[ZOMBIE].head;
  while(p)
  {
    if(p->pid == pid)
    {
      p -> killed = 1;
      release(&ptable.lock);
      return 0;
    }
    p = p -> next; 
  }

//-------------------------//

  release(&ptable.lock);
  return -1;
}


#elif CS333_P3
// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;
  acquire(&ptable.lock);
//--------Added Code-------//
  p = ptable.list[EMBRYO].head;
  while(p)
  {
    if(p->pid == pid)
    {
      p -> killed = 1;
      release(&ptable.lock);
      return 0;
    }
    p = p -> next; 
  }
  p = ptable.list[SLEEPING].head;
  while(p)
  {
    if(p -> pid == pid)
    {
      p -> killed = 1;
      stateListRemove(&ptable.list[SLEEPING], p);
      if(assertState(p, SLEEPING) < 0)
      {
        panic("non-SLEEPING killed process moved to RUNNING");
      }
      p -> state = RUNNABLE;
      stateListAdd(&ptable.list[p->state], p);
      release(&ptable.lock);
      return 0;
    }
    p = p -> next;
  }

  p = ptable.list[RUNNABLE].head;
  while(p)
  {
    if(p->pid == pid)
    {
      p -> killed = 1;
      release(&ptable.lock);
      return 0;
    }
    p = p -> next; 
  }

  p = ptable.list[RUNNING].head;
  while(p)
  {
    if(p->pid == pid)
    {
      p -> killed = 1;
      release(&ptable.lock);
      return 0;
    }
    p = p -> next; 
  }
//-------------------------//

  release(&ptable.lock);
  return -1;
}

//--------------------------
#else
// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#endif

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.

#ifdef CS333_P4
void
procdump(void)
{
  int i;
  struct proc *p;
  char *state;
  uint pc[10];


  cprintf("\n|%s\t|%s\t|%s\t|%s\t|%s\t|%s\t|%s\t|%s\t|%s\t|%s\t|%s\n", "PID", "Name", "UID", "GID", "PPID", "Prio","Elapsed", "CPU", "State", "Size", "PCs");


  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
/*--------------------------------------------------------*/
    #ifdef CS333_P2
    getcallerpcs((uint*)p->context->ebp+2, pc);
    
    int sec = ((ticks - p->start_ticks)/1000);
    int remainder = ((ticks - p->start_ticks) % 1000);
    cprintf("|%d\t|%s\t|%d\t|%d\t",p->pid, p->name, p->uid, p->gid);
    
    if(p -> parent)
      cprintf("|%d\t", p->parent->pid); 
    else
      cprintf("|%d\t", p->pid);

    cprintf("|%d\t", p -> priority);

    cprintf("|%d%s", sec, ".");
    if(remainder<100)
      cprintf("%d",0);
    if(remainder<10) 
      cprintf("%d",0);
    cprintf("%d\t", remainder);
    
    sec = (p -> cpu_ticks_total/1000);
    remainder = (p -> cpu_ticks_total % 1000);
    cprintf("\t|%d%s", sec, ".");
    if(remainder<100)
      cprintf("%d",0);
    if(remainder<10) 
      cprintf("%d",0);
    cprintf("%d\t", remainder);

    
    cprintf("|%s\t|%d\t", state, p->sz);
    for(i=0; i<10 && pc[i] != 0; i++)
      cprintf(" %p", pc[i]);
    cprintf("\n"); 
    

/*--------------------------------------------------------*/
    #elif CS333_P1
    getcallerpcs((uint*)p->context->ebp+2, pc);
    
    int sec = ((ticks - p->start_ticks)/1000);
    int remainder = ((ticks - p->start_ticks) % 1000);
    cprintf("|%d\t|%s\t|%d%s", p->pid, p->name, sec, ".");
    if(remainder<100)
      cprintf("%d",0);
    if(remainder<10) 
      cprintf("%d",0);
    cprintf("%d\t\t|%s\t|%d\t|", remainder, state, p->sz);
    for(i=0; i<10 && pc[i] != 0; i++)
      cprintf(" %p", pc[i]);
    cprintf("\n"); 

    #else
    cprintf("%d\t%s\t%s\t", p->pid, p->name, state);


    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");

    #endif
  }
}


#else
void
procdump(void)
{
  int i;
  struct proc *p;
  char *state;
  uint pc[10];


  #ifdef CS333_P2
  cprintf("\n|%s\t|%s\t|%s\t|%s\t|%s\t|%s\t|%s\t|%s\t|%s\t|%s\n", "PID", "Name", "UID", "GID", "PPID", "Elapsed", "CPU", "State", "Size", "PCs");
  #elif CS333_P1
  cprintf("\n|%s\t|%s\t|%s\t|%s\t|%s\t|%s\n", "PID", "Name", "Elapsed", "State", "Size", "PCs");
  #endif 


  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
/*--------------------------------------------------------*/
    #ifdef CS333_P2
    getcallerpcs((uint*)p->context->ebp+2, pc);
    
    int sec = ((ticks - p->start_ticks)/1000);
    int remainder = ((ticks - p->start_ticks) % 1000);
    cprintf("|%d\t|%s\t|%d\t|%d\t",p->pid, p->name, p->uid, p->gid);
    
    if(p -> parent)
      cprintf("|%d\t", p->parent->pid); 
    else
      cprintf("|%d\t", p->pid);

    cprintf("|%d%s", sec, ".");
    if(remainder<100)
      cprintf("%d",0);
    if(remainder<10) 
      cprintf("%d",0);
    cprintf("%d\t", remainder);
    
    sec = (p -> cpu_ticks_total/1000);
    remainder = (p -> cpu_ticks_total % 1000);
    cprintf("\t|%d%s", sec, ".");
    if(remainder<100)
      cprintf("%d",0);
    if(remainder<10) 
      cprintf("%d",0);
    cprintf("%d\t", remainder);

    
    cprintf("|%s\t|%d\t", state, p->sz);
    for(i=0; i<10 && pc[i] != 0; i++)
      cprintf(" %p", pc[i]);
    cprintf("\n"); 
    

/*--------------------------------------------------------*/
    #elif CS333_P1
    getcallerpcs((uint*)p->context->ebp+2, pc);
    
    int sec = ((ticks - p->start_ticks)/1000);
    int remainder = ((ticks - p->start_ticks) % 1000);
    cprintf("|%d\t|%s\t|%d%s", p->pid, p->name, sec, ".");
    if(remainder<100)
      cprintf("%d",0);
    if(remainder<10) 
      cprintf("%d",0);
    cprintf("%d\t\t|%s\t|%d\t|", remainder, state, p->sz);
    for(i=0; i<10 && pc[i] != 0; i++)
      cprintf(" %p", pc[i]);
    cprintf("\n"); 

    #else
    cprintf("%d\t%s\t%s\t", p->pid, p->name, state);


    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");

    #endif
  }
}
#endif

#ifdef CS333_P2

int
copyProcTable(uint max, struct uproc* table) //gets process info
{
  if(!table)
  {
    return -1;
  }
  acquire(&ptable.lock);

  int count = 0;
  struct proc * p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(count >= max)
    {
        break;
    }
    if(p -> state != UNUSED && p -> state != EMBRYO)
    {
      table[count].pid = p -> pid;
      table[count].uid = p -> uid;
      table[count].gid = p -> gid;
      if(p->parent)
      {
        table[count].ppid = p -> parent -> pid;
      }
      else
      {
        table[count].ppid = p -> pid;
      }

      #ifdef CS333_P4
      table[count].priority = p -> priority;
      #endif
      table[count].elapsed_ticks = ticks - p -> start_ticks;
      table[count].CPU_total_ticks = p -> cpu_ticks_total;
      safestrcpy(table[count].state, states[p->state], STRMAX);
      table[count].size = p -> sz;
      safestrcpy(table[count].name, p->name, sizeof(p->name));
      //safestrcpy(destination, scource, sizeof scource);

      ++count;
   
    }
  }
  release(&ptable.lock);
  return count;

}


#endif


/*  uint pid;
  uint uid;
  uint gid;
  uint ppid;
  uint priority;
  uint elapsed_ticks;
  uint CPU_total_ticks;
  char state[STRMAX];
  uint size;
  char name[STRMAX];*/

#ifdef CS333_P4

void
procR(void)//displays processes in the RUNNABLE list
{
  struct proc * p;
  
  acquire(&ptable.lock);
  
  p = ptable.ready[MAXPRIO].head;
  if(!p)
  {
    cprintf("%s", "No Runnable Proccesses\n$ ");
    release(&ptable.lock);
    return;
  }

  cprintf("%s\n", "Ready list processes: ");

  for(int i = MAXPRIO; i >= 0; --i)
  {
    p = ptable.ready[i].head;
   
    if(i == MAXPRIO)
      cprintf("%s", "MAXPRIO: ");

    else 
    {
      cprintf("\n%s%d%s", "MAXPRIO - ", MAXPRIO - i, ": "); 
    }

    while(p)
    {
      if(!p -> next)
      {
        cprintf("(%d%s%d)\n", p -> pid, ", ", p -> budget);
        break;
      }
      cprintf("(%d%s%d)%s", p -> pid, ", ", p -> budget, " -> ");
      p = p -> next;
    }

  }
  cprintf("%s", "\n$ ");

  release(&ptable.lock);


}

#elif CS333_P3

void
procR(void)//displays processes in the RUNNABLE list
{
  struct proc * p;
  
  acquire(&ptable.lock);
  
  p = ptable.list[RUNNABLE].head;
  if(!p)
  {
    cprintf("%s", "No Runnable Proccesses\n$ ");
    release(&ptable.lock);
    return;
  }

  cprintf("%s\n", "Ready list processes: ");

  while(p)
  {
    if(!p -> next)
    {
      cprintf("%d", p -> pid);
      break;
    }
    cprintf("%d %s ", p -> pid, "->");
    p = p -> next;
  }
  
  cprintf("%s", "\n$ ");

  release(&ptable.lock);

}
#endif

#ifdef CS333_P3

void
procF(void)//displays processes in the FREE list
{
  struct proc * p;
  int counter = 0;
  
  acquire(&ptable.lock);
  
  p = ptable.list[UNUSED].head;
  if(!p)
  {
    cprintf("%s", "No Free/Unused Processes\n$ ");
    release(&ptable.lock);
    return;
  }
  while(p)
  {
    ++counter;
    p = p -> next;
  }

  cprintf("%s %d %s ", "Free List Size: ", counter, " processes\n$ ");

  release(&ptable.lock);

}

void
procS(void)//displays processes in the SLEEPING list
{
  struct proc * p;
  
  acquire(&ptable.lock);
  
  p = ptable.list[SLEEPING].head;
  if(!p)
  {
    cprintf("%s", "No Sleeping Proccesses\n$ ");
  }

  cprintf("%s\n", "Sleeping list processes: ");

  while(p)
  {
    if(!p -> next)
    {
      cprintf("%d", p -> pid);
      break;
    }
    cprintf("%d %s ", p -> pid, "->");
    p = p -> next;
  }
 
  cprintf("%s", "\n$ ");

  release(&ptable.lock);

}

void
procZ(void)//displays processes in the ZOMBIE list
{
  struct proc * p;
  
  acquire(&ptable.lock);
  
  p = ptable.list[ZOMBIE].head;
  if(!p)
  {
    cprintf("%s", "No Zombie Proccesses\n$ ");
  }
  while(p)
  {
    if(!p -> next)
    {
      if(!p -> parent)
      {
        cprintf("%s%d%s", "(", p->pid, ",(No Parent)\n$ ");
      }
      else{
        cprintf("%s%d%s%d%s", "(", p->pid, ",", p->parent->pid, ")\n$ ");
      }
    }
    else{
      cprintf("%s%d%s%d%s%s", "(", p->pid, ",", p->parent->pid, ")", "->");
    }
    p = p -> next;
  }

  release(&ptable.lock);

}

#endif

#ifdef CS333_P4
int
setpriority(int pid, int priority)
{
  if(pid < 0 || priority < 0)
    return -1;

  if(priority > (MAXPRIO))
    return -1;

  acquire(&ptable.lock);
  
  struct proc * p;

  for(int i = 0; i<MAXPRIO; ++i) 
  {
    p = ptable.ready[i].head;
    while(p)
    {
      if(p -> pid == pid)
      {
        if(assertState(p, RUNNABLE) <0) 
        {
          panic("invalid type encountered changing priority");
        }
        
        if(priority != p -> priority)
        {
          stateListRemove(&ptable.ready[p->priority],p);
          p -> priority = priority;
          p -> budget = BUDGET;
          stateListAdd(&ptable.ready[p->priority],p);
        }
        else {
          p -> budget = BUDGET;
        }

        release(&ptable.lock);
        return 0;
      }  
      p = p -> next; 
    }
  }
  
  p = ptable.list[RUNNING].head;

  while(p)
  {
    if(p -> pid == pid)
    {
      p -> priority = priority;
      p -> budget = BUDGET;
      release(&ptable.lock);
      return 0;
    }  
    p = p -> next; 
  }

  p = ptable.list[SLEEPING].head;

  while(p)
  {
    if(p -> pid == pid)
    {
      p -> priority = priority;
      p -> budget = BUDGET;
      release(&ptable.lock);
      return 0;
    }  
    p = p -> next; 
  }

  release(&ptable.lock);
  return -1;  
}

int
getpriority(int pid)
{
  if(pid < 0)
    return -1;

  acquire(&ptable.lock);
 
  struct proc * p;

  for(int i = 0; i<MAXPRIO; ++i) 
  {
    p = ptable.ready[i].head;
    while(p)
    {
      if(p -> pid == pid)
      {
        release(&ptable.lock);    
        return p -> priority;
      }  
      p = p -> next; 
    }
  }
  
  p = ptable.list[RUNNING].head;

  while(p)
  {
    if(p -> pid == pid)
    {
      release(&ptable.lock);
      return p -> priority;
    }  
    p = p -> next; 
  }

  p = ptable.list[SLEEPING].head;

  while(p)
  {
    if(p -> pid == pid)
    {
      release(&ptable.lock);
      return p -> priority;
    }  
    p = p -> next; 
  }

  release(&ptable.lock);
  return -1;  
}



#endif


#ifdef CS333_P5

int 
chmod(char *pathname, int mode)
{

  return -1;
}

int
chown(char *pathname, int mode)
{

  return -1;
}

int
chgrp(char *pathname, int group)
{

  return -1;
}

#endif






