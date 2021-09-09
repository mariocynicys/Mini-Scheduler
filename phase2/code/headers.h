#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef short bool;
#define true 1
#define false 0

#define SHKEY 300
#define COM_GEN_SCH 400

// MAX_PROC_TABLE_SIZE needs to be powers of 2, that is because [(unsinged
// int)(-1) % (powerOf2Number) = powerOf2Numer-1], this serves when the 0
// overflows to -1, it goes back to (MAX_PROC_TABLE_SIZE-1) And we use this perk
// in many places in our code to keep it shorter and clean TL;DR use 2, 4, 8,
// ... , 512, 1024, and so on
#define MAX_PROC_TABLE_SIZE 1024
#define MAX_MEM_SIZE 1024

union Semun {
  int val;               /* value for SETVAL */
  struct semid_ds *buf;  /* buffer for IPC_STAT & IPC_SET */
  unsigned short *array; /* array for GETALL & SETALL */
  struct seminfo *__buf; /* buffer for IPC_INFO */
  void *__pad;
};

struct process {
  unsigned int _id;
  unsigned int arv;
  unsigned int run;
  unsigned int pri;
  unsigned int mem;
};
///==============================
// don't mess with this variable//
int *shmaddr; //
//===============================

int getClk() { return *shmaddr; }

/*
 * All processes call this function at the beginning to establish communication
 * between them and the clock module. Again, remember that the clock is only
 * emulation!
 */
void initClk() {
  int shmid = shmget(SHKEY, 4, 0444);
  while ((int)shmid == -1) {
    // Make sure that the clock exists
    // printf("Wait! The clock not initialized yet!\n");// useless
    // sleep(1);// might get the system out of sync
    shmid = shmget(SHKEY, 4, 0444);
  }
  shmaddr = (int *)shmat(shmid, (void *)0, 0);
}

/*
 * All processes call this function at the end to release the communication
 * resources between them and the clock module.
 * Again, Remember that the clock is only emulation!
 * Input: terminateAll: a flag to indicate whether that this is the end of
 * simulation. It terminates the whole system and releases resources.
 */

void destroyClk(bool terminateAll) {
  shmdt(shmaddr);
  if (terminateAll) {
    killpg(getpgrp(), SIGINT);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SOME HELPER FUNCTIONS///SOME HELPER FUNCTIONS///SOME HELPER FUNCTIONS///SOME
/// HELPER FUNCTIONS///
///////////////////////////////////////////////////////////////////////////////////////////////////
/// SOME HELPER FUNCTIONS///SOME HELPER FUNCTIONS///SOME HELPER FUNCTIONS///SOME
/// HELPER FUNCTIONS///
///////////////////////////////////////////////////////////////////////////////////////////////////
/// SOME HELPER FUNCTIONS///SOME HELPER FUNCTIONS///SOME HELPER FUNCTIONS///SOME
/// HELPER FUNCTIONS///
///////////////////////////////////////////////////////////////////////////////////////////////////

// Descripes the state of one process
enum State { RUNNING, WAITING, NOTSTARTED };

// This function is used to init the communication between the process generator
// and the scheduler, we are using a shared mem + semaphores bcz we hate msg
// queues this fucntion is only called by process_generator.c
void initComGenSch() {
  // Shared memory
  int shm = shmget(COM_GEN_SCH, sizeof(struct process), 0666 | IPC_CREAT);

  // Semaphroe to act as a lock
  int sem = semget(COM_GEN_SCH, 1, 0666 | IPC_CREAT);

  if (shm == -1 || sem == -1) {
    perror("Error while creating the communication channel between "
           "Proc_generator & Scheduler");
    exit(-1);
  }

  union Semun semun;
  semun.val = 0;

  // Setting the semaphore as a lock
  semctl(sem, 0, SETVAL, semun);
}

// Downs a semaphore
void down(int sem) {
  struct sembuf p_op;

  p_op.sem_num = 0;
  p_op.sem_op = -1;
  p_op.sem_flg = !IPC_NOWAIT;

  if (semop(sem, &p_op, 1) == -1) {
    perror("Error during down()");
    exit(-1);
  }
}

// Ups a semaphore
void up(int sem) {
  struct sembuf v_op;

  v_op.sem_num = 0;
  v_op.sem_op = +1;
  v_op.sem_flg = !IPC_NOWAIT;

  if (semop(sem, &v_op, 1) == -1) {
    perror("Error during up()");
    exit(-1);
  }
}

// Get the sem made @initComGenSch
int getsem() { return semget(COM_GEN_SCH, 1, 0666); }

// Get the shm made @initComGenSch
int getshm() { return shmget(COM_GEN_SCH, 4, 0666); }

// Get the current working directory
char *get_cwd() {
  char *cwd = getcwd(NULL, 0);
  return cwd;
}