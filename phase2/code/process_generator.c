#include "headers.h"

// Pid of scheduler and clock processes
int sch, clk;
// Used as a counter to index the current coming process
int curr_proc = 0;
// Total number of processes
int proc_count = 0;
// The array of processes
struct process *proc;
// sem and shm id (between sch & proc_gen)
int sem, shm;
// shmaddr2
void *shmaddr2;

void secPassed(int);
void clearResources(int);
void initArgs(int, char *[]);

int main(int argc, char *argv[]) {
  // TODO Initialization
  signal(SIGINT, clearResources);
  signal(SIGUSR1, secPassed);

  initArgs(argc, argv);

  char *cwd = get_cwd();

  // This function is only used in the process_generator
  // Used to setup the communication between the process_generator and the
  // scheduler defined in headers.h
  initComGenSch();

  sem = getsem();
  shm = getshm();
  shmaddr2 = shmat(shm, NULL, 0);

  // This does not assume what so ever that the input file is in the same
  // directory as the process_generator (Ctrl+F "8y:]nF9P")
  char path[512];
  strcpy(path, cwd);
  strcat(strcat(path, "/"), argv[1]);
  // 1. Read the input files.
  FILE *fp = fopen(path, "r");
  char *line = NULL;

  unsigned long int dummy;
  unsigned int comment_count;

  // Count the number of lines in the file fp & count the number of #comments
  while (getline(&line, &dummy, fp) != -1) {
    if (line[0] != '#') {
      proc_count++;
    }
  }

  if (proc_count == 0) {
    // We are doing this because there are things that will crash and segfault
    // if proc_count is zero, just exit gracefully
    clearResources(0);
  }

  rewind(fp);

  proc = (struct process *)malloc(sizeof(struct process) * proc_count);
  for (int i = 0; i < proc_count;) {
    getline(&line, &dummy, fp);
    if (line[0] == '#') {
      continue;
    }
    sscanf(line, "%u %u %u %u %u", &proc[i]._id, &proc[i].arv, &proc[i].run,
           &proc[i].pri, &proc[i].mem);
    i++;
  }

  // free free freeeee
  free(line);

  // 2. Read the chosen scheduling algorithm and its parameters, if there are
  // any from the argument list.

  // 3. Initiate and create the scheduler and clock processes.
  char realcwd[512];
  strcpy(realcwd, cwd);
  // free free freeeee
  free(cwd);

  // In this part, we use the cwd with the argv[0] to know the real cwd and
  // start the scheduler and clock from it (reference "8y:]nF9P")
  strcat(strcat(realcwd, "/"), argv[0]);
  for (int i = strlen(realcwd) - 1; i >= 0; i--) {
    if (realcwd[i] == '/') {
      realcwd[i + 1] = 0;
      break;
    }
  }

  sch = fork();
  if (!sch) {
    char last_proc[12];
    snprintf(last_proc, 12, "%u", proc[proc_count - 1].arv);
    char *var = "1";
    if (argc == 8) {
      var = argv[4];
    }

    char path[512];
    strcpy(path, realcwd);
    strcat(path, "/scheduler.o");

    char *args[] = {argv[2], last_proc, var, argv[3], realcwd, NULL};
    execv(path, args);
  }

  clk = fork();
  if (!clk) {
    char path[512];
    strcpy(path, realcwd);
    strcat(path, "/clk.o");

    char *args[] = {NULL};
    execv(path, args);
  }

  // 4. Use this function after creating the clock process to initialize clock.
  initClk();

  // TODO Generation Main Loop
  // 5. Create a data structure for processes and provide it with its
  // parameters.

  // 6. Send the information to the scheduler at the appropriate time.
  for (;; pause())
    ;

  // 7. Clear clock resources
  return 0;
}

void secPassed(int signum) {
  //	If there is a process to be picked,
  //	push it into the process msg queue (or shared memory, to be decided)
  //	and send a signal to the scheduler to pick it up (SIGUSR2)
  int curr_time = getClk();

  // probably this should be converted to a msg queue
  while (curr_proc < proc_count && proc[curr_proc].arv == curr_time) {
    // Write the process info into the shared memory, will be sent to the
    // scheduler
    memcpy(shmaddr2, &proc[curr_proc], sizeof(struct process));

    // Notify the scheduler that a process has arrived
    kill(sch, SIGUSR2);

    // Stop seeking for any more arriving processes till the scheduler
    // picks the one in the shm
    down(sem);

    // Get the next arriving process in curr_time if there is any
    curr_proc++;
  }

  // Notify the scheduler that a second has passed
  kill(sch, SIGUSR1);
}

void clearResources(int signum) {
  // TODO Clears all resources in case of interruption
  destroyClk(false);
  shmdt(shmaddr2);

  shmctl(shm, IPC_RMID, NULL); // delete the shared memory
  semctl(sem, 0, IPC_RMID);    // delete the semaphore set

  wait(&sch);
  wait(&clk);

  // free free freeeee
  free(proc);

  printf("System shutting, bye bye\n");

  exit(0);
}

void initArgs(int count, char *args[]) {
  // ./p_g procs -sch 4 -mem 3
  // or
  // ./p_g procs -q 4 -mem 4 -sch 5

  char *ind_s = 0;
  char *ind_m = 0;
  char *ind_q = 0;

  for (int i = 2; i < count; i++) {
    if (args[i][0] == '-') {
      if (args[i][1] == 's') {
        ind_s = args[i + 1];
      } else if (args[i][1] == 'm') {
        ind_m = args[i + 1];
      } else if (args[i][1] == 'q') {
        ind_q = args[i + 1];
      }
    }
  }

  if (ind_s != 0) {
    args[2] = ind_s;
  }
  if (ind_m != 0) {
    args[3] = ind_m;
  }
  if (ind_q != 0) {
    args[4] = ind_q;
  }
}
