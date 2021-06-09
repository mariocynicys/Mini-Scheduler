#include "headers.h"

// CWD
char *cwd;
// Process Control Block
struct PCB
{
	enum State state;
	struct process data;
	unsigned int pid;
	unsigned int exe_t;
	unsigned int rem_t;
	unsigned int wit_t;
};
// An array representing the process table in the scheduler
// if this array is full, any arriving process can't be stored
// nowhere, so it is discarded
struct PCB pcb[MAX_PROC_TABLE_SIZE];
// To count how many round robin cycles a process has taken
unsigned int rr_c = 0;
// A context switching may happen when rr_c == cnx_c
unsigned int cnx_c = 1;
// For stats
unsigned int idle = 0, tot_procs = 0;
double tot_wta = 0, tot_wait = 0;
// To keep track of where to allocate new processes
unsigned int pcb_ind = 0;
// Current process that the scheduler is running
unsigned int pcb_curr = 0;
// Tracks how many pcbs in the process table
unsigned int pcb_count = 0;
// Algorithm used
unsigned int algo = -1;
// The time at which the last process gonna arrive, used to terminate the scheduler after the set of input processes are finished
unsigned int last = -1;
// Shared mem address (between process_generator.c and scheduler.c)
void *shmaddr2;
// Semaphore id (between process_generator.c and scheduler.c)
int sem, shm;
// Defaul NO_PROCESS PCB value
struct PCB no_pcb = {
		NOTSTARTED,
		{-1, 0, 0, 0},
		-1, 0, 0, 0
	};
// Exits gracefully
void exxit(int);
// Get a newly coming process
void proc_incoming(int);
// Fires the right sch algo when a clk second passes
void a_second_has_passed(int);
// Perf logger
void perf();
FILE* fp;

int main(int argc, char *argv[])
{
	signal(SIGUSR2, proc_incoming);
	signal(SIGUSR1, a_second_has_passed);
	signal(SIGINT, exxit);
	// Set the whole process table to no_pcb (means this slot is free)
	for (int i = 0; i < MAX_PROC_TABLE_SIZE; i++)
	{
		pcb[i] = no_pcb;
	}

	shm = getshm();
	sem = getsem();
	shmaddr2 = shmat(shm, NULL, 0);

	algo = atoi(argv[0]);
	last = atoi(argv[1]);
	fp = fopen("scheduler.log", "w");

	// If Algo is Round Robin
	if(algo == 5)
	{
		// Get the quantum time from the command line
		cnx_c = atoi(argv[2]);
	}

	cwd = get_cwd();
	initClk();

	//TODO: implement the scheduler.
	while (getClk() <= last || pcb_count != 0)
	{
		pause();
	}

	perf();
	//TODO: upon termination release the clock resources.
	shmdt(shmaddr2);
	// free free freeeee
	free(cwd);

	destroyClk(true);
}
// Used by FCFS
char next(int* pcb_cur)
{
	if(pcb_count == 0)
	{
		return 0;
	}
	while (pcb[(*pcb_cur)].data._id == -1)
	{
		(*pcb_cur)++;
		(*pcb_cur) %= MAX_PROC_TABLE_SIZE;
	}
	return 1;
}
// Used by SJF
char next_shortest(int* pcb_cur)
{
	if(pcb_count == 0)
	{
		return 0;
	}
	unsigned int min = -1;
	unsigned int arv = -1;

	// j < pcb_count instead for i < MAX_PROC_TABLE_SIZE for optimization
	for (int i = *pcb_cur, j = 0; j < pcb_count; i++, i %= MAX_PROC_TABLE_SIZE)
	{
		if(pcb[i].data._id != -1)
		{
			if(min > pcb[i].data.run)
			{
				*pcb_cur = i;
				min = pcb[i].data.run;
				arv = pcb[i].data.arv;
			}
			// If the runtime equals, pick the older
			else if(min == pcb[i].data.run && arv > pcb[i].data.arv)
			{
				*pcb_cur = i;
				arv = pcb[i].data.arv;
			}
			j++;
		}
	}
	return 1;
}
// Used by SRTP
char next_shortest_remaining(int* pcb_cur)
{
	if(pcb_count == 0)
	{
		return 0;
	}
	unsigned int min = -1;
	unsigned int arv = -1;

	for (int i = *pcb_cur, j = 0; j < pcb_count; i++, i %= MAX_PROC_TABLE_SIZE)
	{
		if(pcb[i].data._id != -1)
		{
			if(min > pcb[i].rem_t)
			{
				*pcb_cur = i;
				min = pcb[i].rem_t;
				arv = pcb[i].data.arv;
			}
			// If the runtime equals, pick the older
			else if(min == pcb[i].rem_t && arv > pcb[i].data.arv)
			{
				*pcb_cur = i;
				arv = pcb[i].data.arv;
			}
			j++;
		}
	}
	return 1;
}
// Used by RR
char next_rr(int* pcb_cur)
{
	if(pcb_count == 0)
	{
		return 0;
	}
	(*pcb_cur)++;
	(*pcb_cur) %= MAX_PROC_TABLE_SIZE;
	return next(pcb_cur);
}
// Used by HPF
char next_pri(int* pcb_cur)
{
	if(pcb_count == 0)
	{
		return 0;
	}

	unsigned int min = -1;
	unsigned int arv = -1;

	for (int i = *pcb_cur, j = 0; j < pcb_count; i++, i %= MAX_PROC_TABLE_SIZE)
	{
		if(pcb[i].data._id != -1)
		{
			if(min > pcb[i].data.pri)
			{
				*pcb_cur = i;
				min = pcb[i].data.pri;
				arv = pcb[i].data.arv;
			}
			// If the priority equals, pick the older
			else if(min == pcb[i].data.pri && arv > pcb[i].data.arv)
			{
				*pcb_cur = i;
				arv = pcb[i].data.arv;
			}
			j++;
		}
	}
	return 1;
}

void exxit(int signum)
{
	// Because SIGINT exits with a non-zero exit code
	// This is no critical thing, but a non-zero exit code is highlighted in my terminal with a red (x) letter, and it's annoying
	destroyClk(true);
	exit(0);
}

void create_new_pcb(struct process *new_p, short is_rr)
{
	if (pcb_count == MAX_PROC_TABLE_SIZE)
	{
		// Don't put the process in the proc_table if it is full
		return;
	}

	if(is_rr == 0)
	{
		// Find the next empty slot
		while (pcb[pcb_ind].data._id != -1)
		{
			pcb_ind++;
			pcb_ind %= MAX_PROC_TABLE_SIZE;
		}
	}
	else
	{
		// If the algo is RR, we need to put the new pcb right before the pcb_curr
		pcb_ind = pcb_curr;
		// If the slot just before pcb_curr is free, use it
		pcb_ind--;
		pcb_ind %= MAX_PROC_TABLE_SIZE;
		// If it's not free, shift from pcb_curr to the nearest free slot by 1, so to free pcb_curr and put the new process in it
		if(pcb[pcb_ind].data._id != -1)
		{
			while (pcb[pcb_ind].data._id != -1)
			{
				pcb_ind++;
				pcb_ind %= MAX_PROC_TABLE_SIZE;
			}
			// pcb_ind now points to a free slot
			while(pcb_ind != pcb_curr)
			{
				pcb[pcb_ind] = pcb[(pcb_ind-1) % MAX_PROC_TABLE_SIZE];
				pcb_ind--;
				pcb_ind %= MAX_PROC_TABLE_SIZE;
			}
			// The pcb_curr is now @ the place after, at pcb_ind+1
			pcb_curr++;
			pcb_curr %= MAX_PROC_TABLE_SIZE;
		}
	}

	pcb[pcb_ind] = (struct PCB){
			NOTSTARTED,
			{
					new_p->_id,
					new_p->arv,
					new_p->run,
					new_p->pri,
			},
			-1,
			0,
			new_p->run,
			0
		};
	tot_procs++;
	pcb_count++;
}

void proc_incoming(int signum)
{
	create_new_pcb(shmaddr2, algo == 5);

	up(sem);
}

void start(struct PCB *pcb_p)
{
	// Fork a new process
	pcb_p->pid = fork();
	if (!pcb_p->pid)
	{
		// Convert process args to strings
		// 12 because the max num of chars in an unsigned int is 11
		char data[12];
		snprintf(data, 12, "%u\t", pcb_p->data.run);

		char *args[] = {data, NULL};

		char path[512];
		strcpy(path, cwd);
		strcat(path, "/process.o");
		execv(path, args);
	}
	int clk = getClk();
	pcb_p->state = RUNNING;
	fprintf(fp, "At time %u\tprocess %u\tstarted arr %u\ttotal %u\tremain %u\twait %u\n", clk, pcb_p->data._id, pcb_p->data.arv, pcb_p->data.run, pcb_p->rem_t, clk-pcb_p->data.arv-pcb_p->exe_t);
}

void resume(struct PCB *pcb_p)
{
	int clk = getClk();
	pcb_p->state = RUNNING;
	fprintf(fp, "At time %u\tprocess %u\tresumed arr %u\ttotal %u\tremain %u\twait %u\n", clk, pcb_p->data._id, pcb_p->data.arv, pcb_p->data.run, pcb_p->rem_t, clk-pcb_p->data.arv-pcb_p->exe_t);

}

void stop(struct PCB *pcb_p)
{
	int clk = getClk();
	pcb_p->state = WAITING;
	fprintf(fp, "At time %u\tprocess %u\tstopped arr %u\ttotal %u\tremain %u\twait %u\n", clk, pcb_p->data._id, pcb_p->data.arv, pcb_p->data.run, pcb_p->rem_t, clk-pcb_p->data.arv-pcb_p->exe_t);
}

void finish(struct PCB *pcb_p)
{
	int clk = getClk();
	double WTA = (double)(clk-pcb_p->data.arv)/(double)(pcb_p->exe_t?pcb_p->exe_t:1);
	fprintf(fp, "At time %u\tprocess %u\tfinished arr %u\ttotal %u\tremain %u\twait %u\t", clk, pcb_p->data._id, pcb_p->data.arv, pcb_p->data.run, pcb_p->rem_t, clk-pcb_p->data.arv-pcb_p->exe_t);
	fprintf(fp, "TA %u\tWTA %.2f\n", clk-pcb_p->data.arv, WTA);
	tot_wta += WTA;
	tot_wait += (clk-pcb_p->data.arv-pcb_p->exe_t);
	// This can be waitpid(no_pcb->pid, ...), but since this is a 1-core cpu, 1 process at a time, so no worries about which process it's gonna reap
	wait(&clk);
	*pcb_p = no_pcb;
	pcb_count--;
}
// ALGO 1
void FCFS()
{
	if (pcb[pcb_curr].state == RUNNING)
	{
		kill(pcb[pcb_curr].pid, SIGUSR2);
		pcb[pcb_curr].exe_t++;
		pcb[pcb_curr].rem_t--;

		// If the process ends, remove it from the process table pcb
		if (pcb[pcb_curr].rem_t == 0)
		{
			finish(&pcb[pcb_curr]);

			// Point to the next process in the pcb, if there is any
			if(next(&pcb_curr) == 0)
			{
				return;
			}
		}
	}
	// Start the process pointed to by pcb_curr if it's not started
	// That is, if the previous if condition terminates a process, the next process (which is being started down there)
	// should start in the same second
	if (pcb[pcb_curr].state == NOTSTARTED)
	{
		next(&pcb_curr);
		start(&pcb[pcb_curr]);

		// To handle the stupid case when a process takes zero time
		while (pcb[pcb_curr].data.run == 0)
		{
			finish(&pcb[pcb_curr]);
			if(next(&pcb_curr) == 0)
			{
				return;
			}
			start(&pcb[pcb_curr]);
		}
	}
}
// ALGO 2
void SJF()
{
	if (pcb[pcb_curr].state == RUNNING)
	{
		kill(pcb[pcb_curr].pid, SIGUSR2);
		pcb[pcb_curr].exe_t++;
		pcb[pcb_curr].rem_t--;

		// If the process ends, remove it from the process table pcb
		if (pcb[pcb_curr].rem_t == 0)
		{
			finish(&pcb[pcb_curr]);

			// Point to the next process in the pcb, if there is any
			if(next_shortest(&pcb_curr) == 0)
			{
				return;
			}
		}
	}
	// Start the process pointed to by pcb_curr if it's not started
	// That is, if the previous if condition terminates a process, the next process (which is being started down there)
	// should start in the same second
	if (pcb[pcb_curr].state == NOTSTARTED)
	{
		next_shortest(&pcb_curr);
		start(&pcb[pcb_curr]);

		// To handle the stupid case when a process takes zero time
		while (pcb[pcb_curr].data.run == 0)
		{
			finish(&pcb[pcb_curr]);
			if(next_shortest(&pcb_curr) == 0)
			{
				return;
			}
			start(&pcb[pcb_curr]);
		}
	}
}
// ALGO 3
void HPF()
{
	if(pcb[pcb_curr].state == RUNNING)
	{
		kill(pcb[pcb_curr].pid, SIGUSR2);
		pcb[pcb_curr].rem_t--;
		pcb[pcb_curr].exe_t++;

		if(pcb[pcb_curr].rem_t == 0)
		{
			finish(&pcb[pcb_curr]);
			if(next_pri(&pcb_curr) == 0)
			{
				return;
			}
		}
		else
		{
			unsigned pcb_past = pcb_curr;
			next_pri(&pcb_curr);
			if(pcb_curr == pcb_past)
			{
				return;
			}
			stop(&pcb[pcb_past]);
		}
	}

	next_pri(&pcb_curr);
	if(pcb[pcb_curr].state == NOTSTARTED)
	{
		start(&pcb[pcb_curr]);

		// To handle the stupid case when a process takes zero time
		while (pcb[pcb_curr].data.run == 0)
		{
			finish(&pcb[pcb_curr]);
			if(next_pri(&pcb_curr) == 0)
			{
				return;
			}
			if(pcb[pcb_curr].state == WAITING)
			{
				resume(&pcb[pcb_curr]);
			}
			else
			{
				start(&pcb[pcb_curr]);
			}
		}
	}
	else if(pcb[pcb_curr].state == WAITING)
	{
		resume(&pcb[pcb_curr]);
	}
}
// ALGO 4
void SRTN()
{
	if(pcb[pcb_curr].state == RUNNING)
	{
		kill(pcb[pcb_curr].pid, SIGUSR2);
		pcb[pcb_curr].rem_t--;
		pcb[pcb_curr].exe_t++;

		if(pcb[pcb_curr].rem_t == 0)
		{
			finish(&pcb[pcb_curr]);
			if(next_shortest_remaining(&pcb_curr) == 0)
			{
				return;
			}
		}
		else
		{
			unsigned pcb_past = pcb_curr;
			next_shortest_remaining(&pcb_curr);
			if(pcb_curr == pcb_past)
			{
				return;
			}
			stop(&pcb[pcb_past]);
		}
	}

	next_shortest_remaining(&pcb_curr);
	if(pcb[pcb_curr].state == NOTSTARTED)
	{
		start(&pcb[pcb_curr]);

		// To handle the stupid case when a process takes zero time
		while (pcb[pcb_curr].data.run == 0)
		{
			finish(&pcb[pcb_curr]);
			if(next_shortest_remaining(&pcb_curr) == 0)
			{
				return;
			}
			if(pcb[pcb_curr].state == WAITING)
			{
				resume(&pcb[pcb_curr]);
			}
			else
			{
				start(&pcb[pcb_curr]);
			}
		}
	}
	else if(pcb[pcb_curr].state == WAITING)
	{
		resume(&pcb[pcb_curr]);
	}
}
// ALGO 5
void RR()
{
	if(pcb[pcb_curr].state == RUNNING)
	{
		kill(pcb[pcb_curr].pid, SIGUSR2);
		pcb[pcb_curr].rem_t--;
		pcb[pcb_curr].exe_t++;
		rr_c++;

		if(pcb[pcb_curr].rem_t == 0)
		{
			// Reset the Round Robin counter
			rr_c = 0;
			finish(&pcb[pcb_curr]);
			if(next_rr(&pcb_curr) == 0)
			{
				return;
			}
		}
		else
		{
			if(rr_c == cnx_c)
			{
				// Reset the Round Robin counter
				rr_c = 0;
				unsigned int pcb_past = pcb_curr;
				next_rr(&pcb_curr);
				if(pcb_curr == pcb_past)
				{
					return;
				}
				stop(&pcb[pcb_past]);
			}
		}
	}
	// Use next and not next_rr so not to force moving the pcb_curr one step forward
	next(&pcb_curr);
	if(pcb[pcb_curr].state == NOTSTARTED)
	{
		start(&pcb[pcb_curr]);

		// To handle the stupid case when a process takes zero time
		while (pcb[pcb_curr].data.run == 0)
		{
			finish(&pcb[pcb_curr]);
			if(next_rr(&pcb_curr) == 0)
			{
				return;
			}
			if(pcb[pcb_curr].state == WAITING)
			{
				resume(&pcb[pcb_curr]);
			}
			else
			{
				start(&pcb[pcb_curr]);
			}
		}
	}
	else if(pcb[pcb_curr].state == WAITING)
	{
		resume(&pcb[pcb_curr]);
	}
}

void a_second_has_passed(int signum)
{
	if (pcb_count > 0)
	{
		switch (algo)
		{
		case 1:
			FCFS();
			break;
		case 2:
			SJF();
			break;
		case 3:
			HPF();
			break;
		case 4:
			SRTN();
			break;
		case 5:
			RR();
			break;
		}
	}
	else
	{
		idle++;
	}
}

void perf()
{
	fclose(fp);
	fp = fopen("scheduler.perf", "w");
	tot_wait /= tot_procs;
	tot_wta /= tot_procs;
	unsigned int clk = getClk();
	fprintf(fp, "CPU utilization = %.2f%%\nAvg WTA = %.2f\nAvg Waiting = %.2f\n", 100.0*(clk-idle)/clk, tot_wta, tot_wait);
	fclose(fp);
}
