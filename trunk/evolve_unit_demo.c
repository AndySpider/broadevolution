/***************************************************************************
 *   Copyright (C) 2010 by hk                                              *
 *   hkuieagle@gmail.com                                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

/* Vitual evironments, each range from -5 to 5, higher value means more confortable envir */
int a, b, c;
int *envir0 = &a; 
int *envir1 = &b;
int *envir2 = &c;
#define MAX_UNIT 1000		// Max number of evolve_units, change this as you like.

#define DEFAULT_ENVIR2 1	// Default value for envir2
#define DEFAULT_ENVIR1 2
#define DEFAULT_ENVIR0 2 

#define DEFAULT_GENE 7		// Default gene value for the Big-Mother.
#define MUTATION_CHANCE 10	// Default value of 1/(mutation chance) of each gene bit.

int fd[2];			// Pass envir value dynamicly.
int unit_count;			// Total num of alive evolve_units
pthread_mutex_t count_lock;
int unit_per_gene[8];		// Num of specific type of gene. 8 in sum
pthread_mutex_t per_lock;


/* Statement of functions */
static void get_envir(int);
int get_sense0(char *,int *);
int get_sense1(char *,int *);
int get_sense2(char *,int *);
int get_space();
int parse_gene(char *,int);
int generate_gene(char *);
void *evolve_unit(void *);

int main(void)
{
	/*
	 * The Gene.
	 * One character per bit with value 0 or 1.
	 */
	char *gene;
	pid_t pid;
	pthread_t ntid;
	int i;
	
	if (pipe(fd) < 0)		// Create pipe
		exit(1);
	if ((pid = fork()) < 0)
		exit(2);
	else if ( pid == 0)  		// Child
	{
		close(fd[1]);
		*envir0 = DEFAULT_ENVIR0; *envir1 = DEFAULT_ENVIR1; *envir2 = DEFAULT_ENVIR2; //  default values for envir*
		gene = malloc(sizeof(char));
		*gene = (char)DEFAULT_GENE;			// default gene:111 for the Big-Mother!
		signal(SIGHUP, get_envir);
		
		pthread_mutex_init(&count_lock,NULL);
		pthread_mutex_init(&per_lock,NULL);

		pthread_create(&ntid,NULL,evolve_unit,(void *)gene);  
		printf("\n");
		while(1)
		{
			sleep(1);
			if (unit_count == 0)		// all die
			{
				printf("Game	Over!");
				break;
			}
			printf("#-----------------------------------------------#\n");
			printf("(Envir)     %d %d %d\n",*envir2,*envir1,*envir0);
			printf("(Unit_count)			%d\n",unit_count);
			for (i=0; i<8; i++)
			{
				printf("(Gene)      %d %d %d:	%d",i/2/2%2,i/2%2,i%2,unit_per_gene[i]);
				printf("\n");
			}
			printf("#-----------------------------------------------#\n");
		}
		pthread_mutex_destroy(&count_lock);
		pthread_mutex_destroy(&per_lock);
		free(gene);
	}
	else				// Parent
	{
		close(fd[0]);
		while (1)
		{
			printf("Enter envir2, envir1, envir0(-5-5). 	At any time!");
			scanf("%d%d%d",envir2,envir1,envir0);
			kill(pid, SIGHUP);
			printf("Child pid: %d\n",pid);
			write(fd[1],envir2,sizeof(int));
			write(fd[1],envir1,sizeof(int));
			write(fd[1],envir0,sizeof(int));
		}
	}
	exit(0);
}

/* Signal Handler */
static void get_envir(int signo)
{
	signal(SIGHUP, get_envir);
	if (signo == SIGHUP)
		read(fd[0],envir2,sizeof(int));
		read(fd[0],envir1,sizeof(int));
		read(fd[0],envir0,sizeof(int));
}
/************************************************** Sense Begin *********************************************/
/*
 * Evolve_uints use this function to 'feel' 1st sense.
 * The sense0 means: current envir0 value multiple the specific bit of my gene.
 */ 
int get_sense0(char *gene,int *envir0)
{
	int *en = envir0;
	char *ge = gene;
	int re = parse_gene(ge,0) * (*en);
	return re;
}

/* 
 * Evolve_units use this function to 'feel' 2nd sense.
 */
int get_sense1(char *gene,int *envir1)
{
	int * en = envir1;
	char *ge = gene;
	int re = parse_gene(ge,1) * (*en);
	return re;
}

/*
 * The 3rd sense.
 */
int get_sense2(char *gene,int *envir2)
{
	int * en = envir2;
	char *ge = gene;
	int re = parse_gene(ge,2) * (*en);
	return re;
}

/* 
 * As space is finite and limited, only MAX_UNIT evolve_units can live to the max extent. 
 */
int get_space()
{
	if (unit_count > MAX_UNIT) 
		return 0;
	else 
		return 1;
}

/********************************************** Sense End *********************************************/

int parse_gene(char *gene,int which_sense)
{
	char *pgene = gene;
	return (*pgene & (1 << which_sense)) > 0 ? 1 : 0; // 1 means 1, ==0 means 0.
}	

/*
 * Generate my children's gene.
 * Each bit of my gene has 1/MUTATION_CHANCE chance to reverse.
*/
int generate_gene(char *gene)
{
	char *pgene = gene;
	int i;
	int gene_bit[3];
	char re_gene = 0; // 0x00000000
	srand((unsigned)time(0)); // random seed
	for (i = 0; i<3 ; i++)
	{
		gene_bit[i] = parse_gene(pgene,i);
		int j = 1 + rand()%MUTATION_CHANCE;  // random value between 1 and MUTATION_CHANCE
		if (j == 1)
			gene_bit[i] = gene_bit[i] == 0 ? 1 : 0;
		re_gene = re_gene | (gene_bit[i] << i);
	}
	return re_gene;
}
		
/*
 * Thread function
 * It's the little evolve unit which can live and breed.
*/
void *evolve_unit(void *arg)
{
	char *mygene = (char *)arg; // Obtain my gene from parent.
	pthread_t ntid;

	pthread_mutex_lock(&count_lock);
	unit_count++;
	pthread_mutex_unlock(&count_lock);
	
	pthread_mutex_lock(&per_lock);
	unit_per_gene[(int)*mygene]++;
	pthread_mutex_unlock(&per_lock);

	int mysense0 = get_sense0(mygene,envir0);
	int mysense1 = get_sense1(mygene,envir1);
	int mysense2 = get_sense2(mygene,envir2);
	int space_status = get_space();
	
	int total_sense = mysense0 + mysense1 + mysense2; // max value = 5x3=15

	if (space_status == 0 || total_sense <= 0)		// lack of space or envir is too bad for me, exit.
	{
		pthread_mutex_lock(&count_lock);
		unit_count--;
		pthread_mutex_unlock(&count_lock);

		pthread_mutex_lock(&per_lock);
		unit_per_gene[(int)*mygene]--;
		pthread_mutex_unlock(&per_lock);
		
		pthread_exit((void *)0);
	}
	/*
	 * if total_sense = 15,then I'm the most heathy guy,who can be alive for the longest 15s, and have 1-->15 reproduction rate.
	 * Else my living time = total_sense/15 x 15s, reproduction rate = total_sense/15 x 15.
	 */
	int mytime = total_sense*15/15 < 0 ? 0 : total_sense; // second
	int myrate = total_sense*15/15; // 1 parent --> myrate children
	
	srand((unsigned)mygene);
	int first_sleep = rand()%(mytime+1);  // sleep random time firstly which overcomes the sudden death of all.
	sleep(first_sleep);	// Enjoy my live now :).
	/*
	 * Prepare genes for my children, and breed.
	 */
	char child_gene[myrate];
	int i;
	for(i=0; i<myrate; i++)
	{
		child_gene[i] = generate_gene(mygene);
		pthread_create(&ntid, NULL, evolve_unit,(void *)&child_gene[i]);
		srand((unsigned)ntid); // random seed
	}	
	
	sleep(mytime-first_sleep);	// Cherish my last time...

pthread_exit:
	pthread_mutex_lock(&count_lock);
	unit_count--;
	pthread_mutex_unlock(&count_lock);

	pthread_mutex_lock(&per_lock);
	unit_per_gene[(int)*mygene]--;
	pthread_mutex_unlock(&per_lock);
	
	pthread_exit((void *)0);
}
