#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ucontext.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>

#define TIMER_SIGNAL_NO SIGVTALRM

#define CLOCK_ID CLOCK_THREAD_CPUTIME_ID


enum queue {
  QUEUE_LOW_PRIORITY = 0,
  QUEUE_MEDIUM_PRIORITY = 1,
  QUEUE_HIGH_PRIORITY = 2,
  QUEUE_UNDEFINED = 3
};


enum status {
  STATUS_READY = 1,
  STATUS_RUNNING = 2,
  STATUS_BLOCKED = 4
};

enum priority {
  PRIORITY_LOW = 8,
  PRIORITY_MEDIUM = 16,
  PRIORITY_HIGH = 32
};

struct ThreadBlock {
  int id;
  int status;
  int numberOfChildren;

  ucontext_t * context;
  struct ThreadBlock * parent;

  struct ThreadBlock * next;
  struct ThreadBlock * previous;
};

struct UThreadOptions {
  long stackSize;
  long timeSlice;
};

typedef struct UThreadOptions UThreadOptions;

typedef struct ThreadBlock ThreadBlock;

struct UThreadContext {
  UThreadOptions uThreadOptions;

  ThreadBlock * waitingQueueHead;

  ThreadBlock ** queueHeads;

  int currentRunningQueue;

  ucontext_t * mainContext;
  int currentID;
  int isInitialized;
  timer_t timerId;
};

typedef struct UThreadContext UThreadContext;

static __thread UThreadContext * uThreadContext;




