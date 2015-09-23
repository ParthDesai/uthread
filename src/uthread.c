// Copyright 2014 Parth Desai. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Functions here implements the interface defined by uthread library's header file.
// It uses ucontext family system calls and virtual timer signal to implement context
// switch and pre-emption respectively.

#include "uthread.h"

static ThreadBlock * CreateThread(void (*func) (void *),void * argument,long stackSize);
static void ExitRunningThread();
static ucontext_t * CreateContext(void (*func)(void),ucontext_t * linkedContext,
                                  long stackSize,int isArgument,void * argument);

static void InitializeUThreadContext(UThreadOptions options);
static void InitializeVirtualTimer();
static void InitializeSignalHandler();
static void SignalHandler(int signo, siginfo_t * siginfo, void *arg);
static void BlockSignal(int signo);
static void UnblockSignal(int signo);
static void RegisterThreadBlockRelation(ThreadBlock * threadBlock);
static ThreadBlock * AddThreadBlockToList(ThreadBlock * listHead,ThreadBlock * block);
static ThreadBlock * FindThreadBlockFromList(ThreadBlock * listHead,int id);
static ThreadBlock * RemoveThreadBlockFromListUsingID(ThreadBlock ** listHead,int id);
static ThreadBlock * RemoveThreadBlockFromList(ThreadBlock ** listHead,ThreadBlock * threadBlock);
static void ReScheduleThreads(int removeRunningThread);
static ThreadBlock * GetNextReadyThread(int * queue);
static ThreadBlock * GetNextReadyThreadFromQueue(int queue);
static ThreadBlock * GetCurrentlyRunningThread(int * queue);



static ThreadBlock * GetCurrentlyRunningThread(int * queue) {
  if(queue != NULL) {
    *queue = uThreadContext->currentRunningQueue;
  }

  if(uThreadContext->currentRunningQueue == QUEUE_UNDEFINED) {
    return NULL;
  }
  return uThreadContext->queueHeads[uThreadContext->currentRunningQueue];
}


static ThreadBlock * GetNextReadyThreadFromQueue(int queue) {
  if(uThreadContext->currentRunningQueue == QUEUE_UNDEFINED) {
    return uThreadContext->queueHeads[queue];
  }
  else if(uThreadContext->currentRunningQueue == queue) {
    return uThreadContext->queueHeads[queue]->next;
  }
  else {
    return uThreadContext->queueHeads[queue];
  }
}


static ThreadBlock * GetNextReadyThread(int * queue) {
  *queue = QUEUE_UNDEFINED;

  if(uThreadContext->queueHeads[QUEUE_HIGH_PRIORITY] != NULL) {
    *queue = QUEUE_HIGH_PRIORITY;
    return GetNextReadyThreadFromQueue(QUEUE_HIGH_PRIORITY);
  } else if(uThreadContext->queueHeads[QUEUE_MEDIUM_PRIORITY] != NULL) {
    *queue = QUEUE_MEDIUM_PRIORITY;
    return GetNextReadyThreadFromQueue(QUEUE_MEDIUM_PRIORITY);
  } else if(uThreadContext->queueHeads[QUEUE_LOW_PRIORITY] != NULL) {
    *queue = QUEUE_LOW_PRIORITY;
    return GetNextReadyThreadFromQueue(QUEUE_LOW_PRIORITY);
  } else {
    return NULL;
  }
}

static void InitializeMainContext() {
  uThreadContext->mainContext = (ucontext_t *) malloc(sizeof(ucontext_t));
}

static void ReScheduleThreads(int removeRunningThread) {

  int queue;
  int runningQueue;

  ThreadBlock * currentRunningThread = GetCurrentlyRunningThread(&runningQueue);

  if(removeRunningThread && currentRunningThread != NULL) {
    RemoveThreadBlockFromList(&uThreadContext->queueHeads[runningQueue],currentRunningThread);
  }

  ThreadBlock * threadBlock = GetNextReadyThread(&queue);

  if(threadBlock == NULL) {
    uThreadContext->currentRunningQueue = QUEUE_UNDEFINED;
    setcontext(uThreadContext->mainContext);
    return;
  }


  if(threadBlock == currentRunningThread) {
    return;
  }

  if(uThreadContext->currentRunningQueue == QUEUE_UNDEFINED) {
    uThreadContext->currentRunningQueue = queue;
    uThreadContext->queueHeads[queue] = threadBlock;
    InitializeMainContext();

    threadBlock->status = STATUS_RUNNING;

    swapcontext(uThreadContext->mainContext,threadBlock->context);
  } else {
    uThreadContext->currentRunningQueue = queue;
    uThreadContext->queueHeads[queue] = threadBlock;

    threadBlock->status = STATUS_RUNNING;
    currentRunningThread->status = STATUS_READY;

    swapcontext(currentRunningThread->context,threadBlock->context);
  }
}


static ThreadBlock * AddThreadBlockToList(ThreadBlock * listHead,ThreadBlock * block) {
  if(listHead == NULL) {
    listHead = block;
    listHead->next = listHead->previous = listHead;
    return listHead;
  } else {
    ThreadBlock * temp = listHead->previous;
    listHead->previous = block;
    block->next = listHead;
    block->previous = temp;
    temp->next = block;
    return listHead;
  }
}


static ThreadBlock * FindThreadBlockFromList(ThreadBlock * listHead,int id) {
  ThreadBlock * pointer = listHead;
  ThreadBlock * result = NULL;

  if(listHead == NULL) {
    return result;
  }

  do {
    if(pointer->id == id) {
      result = pointer;
      break;
    }
    pointer = pointer->next;
  } while (pointer != listHead);

  return result;
}

static ThreadBlock * RemoveThreadBlockFromListUsingID(ThreadBlock ** listHead,int id) {
  ThreadBlock * threadBlock = FindThreadBlockFromList(*listHead,id);
  return RemoveThreadBlockFromList(listHead,threadBlock);
}

static ThreadBlock * RemoveThreadBlockFromList(ThreadBlock ** listHead,ThreadBlock * threadBlock) {
  if(threadBlock == NULL) {
    return NULL;
  }

  if(threadBlock->next == threadBlock) {
    *listHead = NULL;
    return threadBlock;
  } else {
    threadBlock->next->previous = threadBlock->previous;
    threadBlock->previous->next = threadBlock->next;
    return threadBlock;
  }
}


static void SignalHandler(int signo, siginfo_t * siginfo, void *arg) {
  if(uThreadContext->currentRunningQueue == QUEUE_UNDEFINED) {
    return;
  }
  ReScheduleThreads(0);
}

static void BlockSignal(int signo) {
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask,signo);
  sigprocmask(SIG_BLOCK,&mask,NULL);
}

static void UnblockSignal(int signo) {
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask,signo);
  sigprocmask(SIG_UNBLOCK,&mask,NULL);
}


static void InitializeVirtualTimer() {
  InitializeSignalHandler();

  struct sigevent event;
  event.sigev_signo = TIMER_SIGNAL_NO;
  event.sigev_notify = SIGEV_THREAD_ID;
  event._sigev_un._tid = syscall(SYS_gettid);
  timer_create(CLOCK_ID,&event,&uThreadContext->timerId);

  struct itimerspec timerSpec;
  timerSpec.it_interval.tv_sec = 0;
  timerSpec.it_interval.tv_nsec = uThreadContext->uThreadOptions.timeSlice;
  timerSpec.it_value.tv_sec = 0;
  timerSpec.it_value.tv_nsec = uThreadContext->uThreadOptions.timeSlice;

  timer_settime(uThreadContext->timerId,0,&timerSpec,NULL);

  UnblockSignal(TIMER_SIGNAL_NO);
}

static void InitializeSignalHandler() {
  struct sigaction t;
  t.sa_flags = SA_ONSTACK | SA_SIGINFO;
  t.sa_handler = SignalHandler;
  sigemptyset(&t.sa_mask);
  sigaction(TIMER_SIGNAL_NO,&t,NULL);
}

static void RegisterThreadBlockRelation(ThreadBlock * threadBlock) {
  if(uThreadContext->currentRunningQueue == QUEUE_UNDEFINED) {
    return;
  }
  uThreadContext->queueHeads[uThreadContext->currentRunningQueue]->numberOfChildren++;
  threadBlock->parent = uThreadContext->queueHeads[uThreadContext->currentRunningQueue];
}


int UThreadInit(UThreadOptions uThreadOptions) {
  if(uThreadContext != NULL) {
    free(uThreadContext);
  }
  InitializeUThreadContext(uThreadOptions);
  InitializeVirtualTimer();
  return 1;
}

int UThreadSchedThread(void (*func)(void *),void * argument,int pr,unsigned int stacksize) {
  if(uThreadContext->isInitialized == 0) {
    return -1;
  }

  ThreadBlock * threadBlock = NULL;

  if(stacksize == 0) {
    threadBlock = CreateThread(func,argument,uThreadContext->uThreadOptions.stackSize);
  } else {
    threadBlock = CreateThread(func,argument,stacksize);
  }

  BlockSignal(TIMER_SIGNAL_NO);
  RegisterThreadBlockRelation(threadBlock);

  switch(pr) {
    case PRIORITY_HIGH:
    uThreadContext->queueHeads[QUEUE_HIGH_PRIORITY] = AddThreadBlockToList(uThreadContext->queueHeads[QUEUE_HIGH_PRIORITY],threadBlock);
    break;
    case PRIORITY_MEDIUM:
    uThreadContext->queueHeads[QUEUE_MEDIUM_PRIORITY] = AddThreadBlockToList(uThreadContext->queueHeads[QUEUE_MEDIUM_PRIORITY],threadBlock);
    break;
    case PRIORITY_LOW:
    uThreadContext->queueHeads[QUEUE_LOW_PRIORITY] = AddThreadBlockToList(uThreadContext->queueHeads[QUEUE_LOW_PRIORITY],threadBlock);
    break;
  }

  ReScheduleThreads(0);
  UnblockSignal(TIMER_SIGNAL_NO);
  return threadBlock->id;
  }


void UThreadExitThread() {
  ExitRunningThread();
}

static void InitializeUThreadContext(UThreadOptions options) {
  uThreadContext = (UThreadContext *) malloc(sizeof(UThreadContext));

  uThreadContext->uThreadOptions = options;

  uThreadContext->currentID = 1;
  uThreadContext->mainContext = NULL;
  uThreadContext->waitingQueueHead = NULL;
  uThreadContext->currentRunningQueue = QUEUE_UNDEFINED;

  uThreadContext->queueHeads = (ThreadBlock **) malloc(sizeof(UThreadContext *) * 3);

  uThreadContext->isInitialized = 1;
}



static ucontext_t * CreateContext(void (*func)(void),ucontext_t * linkedContext,long stackSize,int isArgument,void * argument) {

  ucontext_t * context = (ucontext_t *) malloc(sizeof(ucontext_t));
  getcontext(context);

  context->uc_stack.ss_sp = (char *) malloc(stackSize);
  context->uc_stack.ss_size = stackSize;
  context->uc_link = linkedContext;


  if(isArgument == 0) {
    makecontext(context,func,0);
  } else {
    makecontext(context,func,1,argument);
  }
  return context;
}

static void ExitRunningThread() {
  BlockSignal(TIMER_SIGNAL_NO);
  ReScheduleThreads(1);
}

static ThreadBlock * CreateThread(void (*func) (void *),void * argument,long stackSize) {

  ucontext_t * linkedContext = CreateContext(ExitRunningThread,NULL,stackSize,0,NULL);
  ucontext_t * threadContext = CreateContext((void (*) (void))func,linkedContext,stackSize,1,argument);
  ThreadBlock * threadBlock = (ThreadBlock *) malloc(sizeof(ThreadBlock));
  threadBlock->context = threadContext;
  threadBlock->status = STATUS_READY;
  threadBlock->numberOfChildren = 0;
  threadBlock->previous = threadBlock->next = threadBlock->parent = NULL;
  threadBlock->id = uThreadContext->currentID++;

  return threadBlock;
}

