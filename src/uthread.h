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
// This file contains structures and exposed function of the uthread
// To use uthread library in your application
// you need to include this header file.

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




