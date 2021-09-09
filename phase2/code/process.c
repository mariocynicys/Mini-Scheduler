#include "headers.h"

/* Modify this file as needed*/
int remainingtime;

void worked_for_one_second(int signum) {
  remainingtime--;
  // printf("%ds left\n",remainingtime);
}

int main(int agrc, char *argv[]) {
  // SOMETHING SUPER WEIRD, SIGUSR1 DOESN'T WORK FOR SOME REASON
  // IT DOESN'T CALL THE SIGNAL HANDLER, I HAD TO USE SIGUSR2 OR SIGURG
  // (WHAT I THINK IS THAT THIS IS SOME SORT OF STACK CURROPTION DUE TO USING
  // execv 2 TIMES TO CREATE THIS PROCESS, FORM THE PROCESS GENRATOR TO THE
  // SCHEDULER DOWN TO A SINGLE PROCESS)
  signal(SIGUSR2, worked_for_one_second);

  remainingtime = atoi(argv[0]);
  // initClk();

  // TODO The process needs to get the remaining time from somewhere

  while (remainingtime > 0)
    ;

  // destroyClk(false);
  return 0;
}