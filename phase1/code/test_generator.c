#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "headers.h"
#define null 0

struct processData
{
	int arrivaltime;
	int priority;
	int runningtime;
	int id;
};

int main(int argc, char *argv[])
{
	FILE *pFile;
	int no;

	/////////////////////////////////////
	char path[512];
	char* cwd = get_cwd();
	// char* filename[128];
	// printf("Please enter the file name to dump to: ");fflush(stdout);
	// scanf("%s", filename);
	strcpy(path, cwd);
	strcat(strcat(path, "/"), argv[2]);
	printf("%s\n", path);
	pFile = fopen(path, "w");
	no = atoi(argv[1]);
	/////////////////////////////////////

	struct processData pData;
	// printf("Please enter the number of processes you want to generate: ");
	// scanf("%d", &no);
	srand(time(null));

	fprintf(pFile, "#id arrival runtime priority\n");
	pData.arrivaltime = 3;
	for (int i = 1; i <= no; i++)
	{
		//generate Data Randomly
		//[min-max] = rand() % (max_number + 1 - minimum_number) + minimum_number
		pData.id = i;
		pData.arrivaltime += rand() % (4); //processes arrives in order
		pData.runningtime = rand() % (7);
		pData.priority = rand() % (11);
		fprintf(pFile, "%d\t%d\t%d\t%d\n", pData.id, pData.arrivaltime, pData.runningtime, pData.priority);
	}
	fclose(pFile);
}
