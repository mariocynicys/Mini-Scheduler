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
	unsigned int mem_s;
};
// An array representing the process table in the scheduler
// if this array is full, any arriving process can't be stored
// nowhere, so it is discarded
struct PCB pcb[MAX_PROC_TABLE_SIZE];
// An array representing the memory of the system
unsigned int memo[MAX_MEM_SIZE];
// Next fit memory pointer
unsigned int nf = 0;
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
// Memory allocation policy
unsigned int mem_pol = -1;
// The time at which the last process gonna arrive, used to terminate the scheduler after the set of input processes are finished
unsigned int last = -1;
// Shared mem address (between process_generator.c and scheduler.c)
void *shmaddr2;
// Semaphore id (between process_generator.c and scheduler.c)
int sem, shm;
// Defaul NO_PROCESS PCB value
struct PCB no_pcb = {
		NOTSTARTED,
		{-1, 0, 0, 0, 0},
		-1, 0, 0, -1
	};
// Exits gracefully
void exxit(int);
// Get a newly coming process
void proc_incoming(int);
// Fires the right sch algo when a clk second passes
void a_second_has_passed(int);
// Allocates memory space for some process using the selected memory allocation policy
unsigned int allc();
// Perf logger
void perf();
FILE* fp, *mp;

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
	for (int i = 0; i < MAX_MEM_SIZE; i++)
	{
		memo[i] = -1;
	}

	shm = getshm();
	sem = getsem();
	shmaddr2 = shmat(shm, NULL, 0);

	algo = atoi(argv[0]);
	last = atoi(argv[1]);
	cnx_c = atoi(argv[2]);
	mem_pol = atoi(argv[3]);

	fp = fopen("scheduler.log", "w");
	mp = fopen("memory.log", "w");
	
	//fp = stdout;
	cwd = argv[4];//get_cwd();
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
	//free(cwd);

	destroyClk(true);
}
// Used by FCFS // NO EDITS NEEDED FOR MEM
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
	pcb[(*pcb_cur)].mem_s = 0;
	return 1;
}
// Used by SJF // NO EDITS NEEDED FOR MEM
char next_shortest(int* pcb_cur)
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
	pcb[(*pcb_cur)].mem_s = 0;
	return 1;
}
// Used by SRTN
char next_shortest_remaining(int* pcb_cur)
{
	if(pcb_count == 0)
	{
		return 0;
	}
	unsigned int min = -1;
	unsigned int arv = -1;
	unsigned int mem = -1;

	// j < pcb_count instead for i < MAX_PROC_TABLE_SIZE for optimization
	for (int i = *pcb_cur, j = 0; j < pcb_count; i++, i %= MAX_PROC_TABLE_SIZE)
	{
		if(pcb[i].data._id != -1)
		{
			unsigned int mem_temp;
			if(pcb[i].state != NOTSTARTED)
			{
				mem_temp = pcb[i].mem_s;
			}
			else
			{
				mem_temp = allc(pcb[i].data.mem);
			}
			if(mem_temp != -1)
			{
				if(min > pcb[i].rem_t)
				{
					*pcb_cur = i;
					min = pcb[i].rem_t;
					arv = pcb[i].data.arv;
					mem = mem_temp;
				}
				// If the rem_time equals, pick the older
				else if(min == pcb[i].rem_t && arv > pcb[i].data.arv)
				{
					*pcb_cur = i;
					arv = pcb[i].data.arv;
					mem = mem_temp;
				}
			}
			j++;
		}
	}
	if(min != -1)
	{
		pcb[(*pcb_cur)].mem_s = mem;
		return 1;
	}
	return 0;
}
// Used by RR
char next_rr(int* pcb_cur)
{
	if(pcb_count == 0)
	{
		return 0;
	}

	// THIS WON'T CAUSE AND INF LOOP, YOU HAVE ALREADY CHECKED FOR PCB COUNT, THAT MEANS THERE IS AT LEAST ONE PROCESS IN THE TABLE
	// AND IF ALL PROCESSES CAN'T BE ALLOCATED, THE RUNNING PROCESS WILL GET ALLOCATED
	while (true)
	{
		(*pcb_cur)++;
		(*pcb_cur) %= MAX_PROC_TABLE_SIZE;
		if(pcb[(*pcb_cur)].data._id != -1)
		{
			if(pcb[(*pcb_cur)].state != NOTSTARTED)
			{
				return 1;
			}
			else
			{
				pcb[(*pcb_cur)].mem_s = allc(pcb[(*pcb_cur)].data.mem);
				if(pcb[(*pcb_cur)].mem_s != -1)
				{
					return 1;
				}
			}
		}
	}
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
	unsigned int mem = -1;

	for (int i = *pcb_cur, j = 0; j < pcb_count; i++, i %= MAX_PROC_TABLE_SIZE)
	{
		if(pcb[i].data._id != -1)
		{
			unsigned int mem_temp;
			if(pcb[i].state != NOTSTARTED)
			{
				mem_temp = pcb[i].mem_s;
			}
			else
			{
				mem_temp = allc(pcb[i].data.mem);
			}
			if(mem_temp != -1)
			{
				if(min > pcb[i].data.pri)
				{
					*pcb_cur = i;
					min = pcb[i].data.pri;
					arv = pcb[i].data.arv;
					mem = mem_temp;
				}
				// If the priority equals, pick the older
				else if(min == pcb[i].data.pri && arv > pcb[i].data.arv)
				{
					*pcb_cur = i;
					arv = pcb[i].data.arv;
					mem = mem_temp;
				}
			}
			j++;
		}
	}
	if(min != -1)
	{
		pcb[(*pcb_cur)].mem_s = mem;
		return 1;
	}
	return 0;
}

void exxit(int signum)
{
	// Because SIGINT exits with a non-zero exit code
	// This is no critical thing, but a non-zero exit code is highlighted in my terminal with a red (x) letter, and it's annoying
	exit(0);
}

void create_new_pcb(struct process *new_p, short is_rr, short is_bud)
{
	if (pcb_count == MAX_PROC_TABLE_SIZE)
	{
		// Don't put the process in the proc_table if it is full
		return;
	}

	unsigned int memo = new_p->mem;
	// Expand the memory size if buddy sys allocation is used
	if(is_bud == 1)
	{
		int i;
		for (i = 1; i < memo; i*=2);
		memo = i;
	}

	if (memo > MAX_MEM_SIZE/4 )
	{
		// Don't put the process in the proc_table if it needes too much memory, more than 256
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
					memo?memo:1 // If a process takes no memory, force it to take at least one block so it's code segment can be stored somewhere
			},
			-1,
			0,
			new_p->run,
			-1
		};
		int i = pcb_ind;

	tot_procs++;
	pcb_count++;
}

void proc_incoming(int signum)
{
	create_new_pcb(shmaddr2, algo == 5, mem_pol == 4);
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
		snprintf(data, 12, "%u", pcb_p->data.run);

		char *args[] = {data, NULL};

		char path[512];
		strcpy(path, cwd);
		strcat(path, "/process.o");
		execv(path, args);
	}
	int clk = getClk();
	pcb_p->state = RUNNING;

	// Allocating memory for this process
	memo[pcb_p->mem_s] = pcb_p->data.mem;
	fprintf(fp, "At time %u\tprocess %u\tstarted arr %u\ttotal %u\tremain %u\twait %u\n", clk, pcb_p->data._id, pcb_p->data.arv, pcb_p->data.run, pcb_p->rem_t, clk-pcb_p->data.arv-pcb_p->exe_t);
	fprintf(mp, "At time %u\tallocated %u\tbytes for process %u\tfrom %u\tto %u\n", clk, pcb_p->data.mem, pcb_p->data._id, pcb_p->mem_s, pcb_p->mem_s + pcb_p->data.mem - 1);
}

void resume(struct PCB *pcb_p)
{
	int clk = getClk();
	pcb_p->state = RUNNING;
	kill(pcb_p->pid, SIGCONT);
	fprintf(fp, "At time %u\tprocess %u\tresumed arr %u\ttotal %u\tremain %u\twait %u\n", clk, pcb_p->data._id, pcb_p->data.arv, pcb_p->data.run, pcb_p->rem_t, clk-pcb_p->data.arv-pcb_p->exe_t);

}

void stop(struct PCB *pcb_p)
{
	int clk = getClk();
	pcb_p->state = WAITING;
	kill(pcb_p->pid, SIGSTOP);
	fprintf(fp, "At time %u\tprocess %u\tstopped arr %u\ttotal %u\tremain %u\twait %u\n", clk, pcb_p->data._id, pcb_p->data.arv, pcb_p->data.run, pcb_p->rem_t, clk-pcb_p->data.arv-pcb_p->exe_t);
}

void finish(struct PCB *pcb_p)
{
	int clk = getClk();
	//(pcb_p->exe_t>1)?pcb_p->exe_t:1 this assures that not division be zero is happening
	double WTA = (double)(clk-pcb_p->data.arv)/(double)((pcb_p->exe_t>1)?pcb_p->exe_t:1);
	fprintf(fp, "At time %u\tprocess %u\tfinished arr %u\ttotal %u\tremain %u\twait %u\t", clk, pcb_p->data._id, pcb_p->data.arv, pcb_p->data.run, pcb_p->rem_t, clk-pcb_p->data.arv-pcb_p->exe_t);
	fprintf(fp, "TA %u\tWTA %.2f\n", clk-pcb_p->data.arv, WTA);
	fprintf(mp, "At time %u\tfreed %u\tbytes for process %u\tfrom %u\tto %u\n", clk, pcb_p->data.mem, pcb_p->data._id, pcb_p->mem_s, pcb_p->mem_s + pcb_p->data.mem - 1);
	tot_wta += WTA;
	tot_wait += (clk-pcb_p->data.arv-pcb_p->exe_t);
	// Deallocating the process' memory
	memo[pcb_p->mem_s] = -1;
	// This can be waitpid(no_pcb->pid, ...), but since this is a 1-core cpu, 1 process at a time, so no worries about which process it's gonna reap
	wait(&clk);
	*pcb_p = no_pcb;
	pcb_count--;
}
// ALGO 1 // NO MEM // BATCH SYSTEM
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
// ALGO 2 // NO MEM // BATCH SYSTEM
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
// ALGO 3 // MEM
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
			goto hpf_lbl;
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
			goto hpf_lbl;
		}
	}

	next_pri(&pcb_curr);
	hpf_lbl:
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
// ALGO 4 // MEM
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
			goto srtn_lbl;
			//you can add goto lbl; in here so not to recomputed the next_ function again
		}
		else
		{
			unsigned pcb_past = pcb_curr;
			// No need to surroud this next_ function with a zero check, because it either
			// chooses the running process, or choose another that is waiting or should be started
			// what i mean is, that since there is a running process (pcb_past), then this function will
			// never return zero, you have to understand
			next_shortest_remaining(&pcb_curr);
			if(pcb_curr == pcb_past)
			{
				return;
			}
			stop(&pcb[pcb_past]);
			goto srtn_lbl;
			//you can add goto lbl; in here so not to recomputed the next_ function again
		}
	}

	// No need to surroud this next_ function with a zero check, because 
	// if there is no applicable process to be choosed and fit in memory, this section
	// won't be run in the frist place, the conditions where this secion runs are,
	//1- if a process is has just been finished and another process is ready. (no zero return, prove it!)
	//2- if a process is has just been stopped and another process to run. (proved above)
	//3- if no process was running, but there is a process or more in the system.
	// Proving this will give you a deep understanding of the system flow.
	next_shortest_remaining(&pcb_curr);
	srtn_lbl:
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
// ALGO 5 // MEM
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
			goto rr_lbl;
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
				goto rr_lbl;
			}
			return;
		}
	}
	
	next_rr(&pcb_curr);
	rr_lbl:
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
			return FCFS();
		case 2:
			return SJF();
		case 3:
			return HPF();
		case 4:
			return SRTN();
		case 5:
			return RR();
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

unsigned int FF(unsigned int mem)
{
	unsigned int target_mem = 0, ff = 0;
	while (ff < MAX_MEM_SIZE && target_mem != mem)
	{
		if(memo[ff] != -1)
		{
			target_mem = 0;
			ff += memo[ff];
		}
		else
		{
			target_mem++;
			ff++;
		}
	}
	if(target_mem == mem)
	{
		return ff - mem;
	}
	return -1;
}

unsigned int NF(unsigned int mem)
{
	if(nf >= MAX_MEM_SIZE)
	{
		nf = 0;
	}
	unsigned int temp = nf, target_mem = 0;
	while(nf < MAX_MEM_SIZE && target_mem != mem)
	{
		if(memo[nf] != -1)
		{
			target_mem = 0;
			nf += memo[nf];
		}
		else
		{
			target_mem++;
			nf++;
		}
	}
	if(target_mem == mem)
	{
		return nf - mem;
	}
	// Do a First Fit
	nf = FF(mem);
	if(nf == -1)
	{
		// Keep the nf @ its intial position
		nf = temp;
		// No memory space found for mem
		return -1;
	}
	// Update the nf from the FF()
	nf += mem;
	return nf - mem;
}

unsigned int BF(unsigned int mem)
{
	unsigned int space = 0, ff = 0, min_mem = -1, min_fit = -2;
	while (ff < MAX_MEM_SIZE)
	{
		if(memo[ff] != -1)
		{
			if(space < min_mem && space >= mem)
			{
				min_mem = space;
				min_fit = ff;
			}
			space = 0;
			ff += memo[ff];
		}
		else
		{
			space++;
			ff++;
		}
	}
	if(space < min_mem && space >= mem)
	{
		min_mem = space;
		min_fit = ff;
	}

	return min_fit - min_mem;
}

unsigned int BSA(unsigned int mem)
{
	unsigned int space = 0, ff = 0, min_mem = -1, min_fit = -2;
	while (ff < MAX_MEM_SIZE)
	{
		if(memo[ff] != -1)
		{
			if(space < min_mem && space >= mem)
			{
				min_mem = space;
				min_fit = ff;
			}
			space = 0;
			// Move by amount equals the largest of (memo[ff] & mem)
			ff += (memo[ff]>mem)?memo[ff]:mem;
		}
		else
		{
			space++;
			ff++;
		}
	}
	if(space < min_mem && space >= mem)
	{
		min_mem = space;
		min_fit = ff;
	}

	return min_fit - min_mem;
}

unsigned int allc(unsigned int mem)
{
	switch (mem_pol)
	{
	case 1:
		return FF(mem);
	case 2:
		return NF(mem);
	case 3:
		return BF(mem);
	case 4:
		return BSA(mem);
	}
}

// So this is how you will apply the memory allocation policy to the system:
// In each next_ function you assure that there is a process that can be allocated successfully
// And this is just a checker to force the correctness of the allocation, the real allocation will
// Happen in the start function, we may need to provide another parameter to the start function that will
// Be coming from next_ functions so not to recompute the place in memory it should reside on again.
//
// Oh, so one way to think of it is to return the memory address of where to start allocation from next_ functions
// And recieve it in start(struct PBC* pcb_p, int mem_start), the memory start address can be zero sometimes, this will break some 
// Existing functionalities, some checks are performed [if(next(&pcb_curr) == 0)], one way to solve this, is to send the return (mem_start+1)
// And when received by start(struct PBC* pcb_p, int mem_start), it get decremented by 1
