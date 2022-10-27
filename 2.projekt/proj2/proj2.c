#include <semaphore.h>
#include <string.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#define MMAP(pointer){(pointer) = mmap(NULL, sizeof(*(pointer)), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);}
#define UNMAP(pointer) {munmap((pointer), sizeof((pointer)));}
#define mysleep(max) { usleep(1000 * (rand() % (max + 1))); }

FILE *file;

//funkce--------------------
int CheckParam(char *argv);
void cleanup();
void VodikProcess();
void KyslikProcess();
void OpenSemaphores();
void CheckSemaphores();
int CheckFile();
void MapNumber();
void CloseSem();
void UnlinkSem();
void UnmapNumber();
int CheckMap();
void CheckArguments();

//semafory------------------
sem_t *kyslik;
sem_t *vodik;
sem_t *barrier;
sem_t *writing;
sem_t *barrier2;
sem_t *barrier_mutex;
sem_t *Mutex;
sem_t *Time;

//promene------------------
int *ProcessCounter = 0;
int *Oxygen = 0;
int *Hydrogen = 0;
int *MoleculeCounter = 0;
int *BarCounter = 0;
int *createdCounter = 0;
int *OxyQueue = 0;
int *HydroQueue = 0;
bool *Enough;

//----------------------------------------------------------------//

int main(int argc, char *argv[])
{
    
    CheckFile();
    OpenSemaphores();
    CheckSemaphores();
    MapNumber();
    CheckMap();

    if (argc != 5)
    {
        fprintf(stderr, "Incorrect number of arguments\n");
        exit(1);
    }

    //volani fce nam overeni, zda-li je parametr cislo

    int PocetKysliku = CheckParam(argv[1]); //kyslik
    int PocetVodiku = CheckParam(argv[2]); //vodik
    int TI = CheckParam(argv[3]); //cas pro vytvoreni atomu
    int TB = CheckParam(argv[4]); //cas pro vytvoreni molekuly
    CheckArguments(PocetKysliku, PocetVodiku);

    if (PocetKysliku == -1 || PocetVodiku == -1 || TI == -1 || TB == -1) 
    {
        fprintf(stderr, "The argument is not a number\n" ); //parametr neni cislo
        exit(1);
    }

    if(TI < 0 || TI > 1000)
    {
        fprintf(stderr, "Set TI in between 0 and 1000\n"); //nastaveni spravneho intervalu pro TI
        exit(1);
    }

    if(TB < 0 || TB > 1000)
    {
        fprintf(stderr, "Set TB in between 0 and 1000\n"); //nastaveni spravneho intervalu pro TB
        exit(1);
    }
    (*ProcessCounter)++;
    (*MoleculeCounter)++;
    

    for (int i = 0; i < PocetKysliku; i++)
    {
    pid_t OProc = fork();
        if (OProc == 0)
        {
            KyslikProcess(TI, i, TB);
            exit(0);
        }
        else if (OProc < 0)
        {
            fprintf(stderr, "Chyba pri vyvoreni kysliku"); 
            exit(1);
        }
    }
    
    for (int i = 0; i < PocetVodiku; i++)
    {
    pid_t HProc = fork();
        if (HProc == 0)
        {
            VodikProcess(TI, i);
            exit(0);
        }
        else if (HProc < 0)
        {
            fprintf(stderr, "Chyba pri vytvoreni vodiku"); 
            exit(1);
        }
    }
    
    while (wait(NULL) > 0);
    cleanup();
    exit(0);
    return 0;

}

void KyslikProcess(int AtomTime, int i, int MoleculeTime)
{
    
    int tmp = i;
    tmp++;
    sem_wait(writing);
    fprintf(file, "%d: O %d: started\n", *ProcessCounter, tmp);
    fflush(file);
    (*ProcessCounter)++;
    (*OxyQueue)++;
    sem_post(writing);

    mysleep(AtomTime);
    
    sem_wait(writing);
    fprintf(file, "%d: O %d: going to queue\n", *ProcessCounter, tmp);
    fflush(file);
    (*ProcessCounter)++;
    sem_post(writing);

    sem_wait(Mutex);
    (*Oxygen)++;
    if ((*Hydrogen) >= 2)
    {
            sem_post(vodik);
            sem_post(vodik);
            (*Hydrogen)-=2;
            sem_post(kyslik);
            (*Oxygen)--;
    }
    else
    {
        sem_post(Mutex);
    }
    sem_wait(kyslik);
    if (*Enough)
    {
        fprintf(file, "%d: O %d: not enough H\n", *ProcessCounter, tmp);
        fflush(file);
        (*ProcessCounter)++;
        exit(1);
    }

    sem_wait(writing);
    fprintf(file, "%d: O %d: creating molecule %d\n", *ProcessCounter, tmp, *MoleculeCounter);
    fflush(file);
    (*ProcessCounter)++;
    sem_post(writing);

    mysleep(MoleculeTime);
    sem_post(Time);
    sem_post(Time);
    
    sem_wait(writing);
    fprintf(file, "%d: O %d: molecule %d created\n", *ProcessCounter, tmp, *MoleculeCounter);
    fflush(file);
    (*ProcessCounter)++;
    (*createdCounter)++;
    
    sem_post(writing);
    if ((*createdCounter) == 3)
    {
        
        (*createdCounter) = 0;
        (*MoleculeCounter)++;
    }
    
    sem_wait(barrier_mutex);
    (*BarCounter)++;
    if ((*BarCounter) == 3)
    {
        sem_wait(barrier2);
        sem_post(barrier);
    }
    sem_post(barrier_mutex);

    sem_wait(barrier);
    sem_post(barrier);

    sem_wait(barrier_mutex);
    (*BarCounter)--;
    if (*BarCounter == 0)
    {
        sem_wait(barrier);
        sem_post(barrier2);
    }
    sem_post(barrier_mutex);

    sem_wait(barrier2);
    sem_post(barrier2);
    
    if ((*OxyQueue) == *MoleculeCounter-1 || (*HydroQueue) == *MoleculeCounter-1 * 2 || *HydroQueue - (*MoleculeCounter-1 * 2) <= 1)
    {
        *Enough = true;
        for (int i = 0; i < *OxyQueue; i++)
        {
            sem_post(kyslik);
        }
        for (int i = 0; i < *HydroQueue; i++)
        {
            sem_post(vodik);
        }
    }
    
    sem_post(Mutex);
   

}
void VodikProcess(int AtomTime, int i)
{
    
    int tmp = i;
    tmp++;
    sem_wait(writing);
    fprintf(file, "%d: H %d: started\n", *ProcessCounter, tmp);
    fflush(file);
    (*ProcessCounter)++;
    (*HydroQueue)++;
    sem_post(writing);

    mysleep(AtomTime);

    sem_wait(writing);
    fprintf(file, "%d: H %d: going to queue\n", *ProcessCounter, tmp);
    fflush(file);
    (*ProcessCounter)++;   
    sem_post(writing);

    sem_wait(Mutex);
    
    (*Hydrogen)++;
    if ((*Hydrogen) >= 2 && (*Oxygen) >= 1)
    {
            sem_post(vodik);
            sem_post(vodik);
            (*Hydrogen)-=2;
            sem_post(kyslik);
            (*Oxygen)--;
            
    }
    else
    {
        sem_post(Mutex);
    }
    
    sem_wait(vodik);
    if (*Enough)
    {
        fprintf(file, "%d: H %d: not enough O or H\n", *ProcessCounter, tmp);
        fflush(file);
        (*ProcessCounter)++;
        exit(1);
    }

    sem_wait(writing);
    fprintf(file, "%d: H %d: creating molecule %d\n", *ProcessCounter, tmp, *MoleculeCounter);
    fflush(file);
    (*ProcessCounter)++;
    sem_post(writing);

    sem_wait(Time);

    sem_wait(writing);
    fprintf(file, "%d: H %d: molecule %d created\n", *ProcessCounter, tmp, *MoleculeCounter);
    fflush(file);
    (*ProcessCounter)++;
    (*createdCounter)++;
    sem_post(writing);

    if ((*createdCounter) == 3)
    {
        (*createdCounter) = 0;
        (*MoleculeCounter)++;
    }

    sem_wait(barrier_mutex);
    (*BarCounter)++;
    if (*BarCounter == 3)
    {
        sem_wait(barrier2);
        sem_post(barrier);
    }
    sem_post(barrier_mutex);

    sem_wait(barrier);
    sem_post(barrier);


    sem_wait(barrier_mutex);
    (*BarCounter)--;
    if (*BarCounter == 0)
    {
        sem_wait(barrier);
        sem_post(barrier2);
    }
    sem_post(barrier_mutex);

    sem_wait(barrier2);
    sem_post(barrier2);
    
    

}

void OpenSemaphores()
{
    Mutex = sem_open("xstipe02Mutex", O_CREAT, 0666, 1);
    kyslik = sem_open("xstipe02Kyslik", O_CREAT, 0666, 0);
    vodik = sem_open("xstipe02Vodik", O_CREAT, 0666,0);
    barrier = sem_open("xstipe02Barrier", O_CREAT, 0666, 0);
    barrier2 = sem_open("xstipe02Barrier2", O_CREAT, 0666, 1);
    barrier_mutex = sem_open("xstipe02BarMutex", O_CREAT, 0666, 1);
    writing = sem_open("xstipe02Writing", O_CREAT, 0666, 1);
    Time = sem_open("xstipe02Time", O_CREAT, 0666, 0);
}
void CheckSemaphores()
{
    if (kyslik == SEM_FAILED || vodik == SEM_FAILED || Mutex == SEM_FAILED || barrier == SEM_FAILED || barrier2 == SEM_FAILED || barrier_mutex == SEM_FAILED ||writing == SEM_FAILED)
    {
        perror("Chyba pri otevirani semaforu");
        exit(1);
    }
}
int CheckFile()
{
    if((file=fopen("proj2.out", "w")) == NULL)
    {
        fprintf(stderr, "Chyba pri otevirani souboru");
        exit(1);
    }
    return 0;
}
int CheckParam(char *argv) //overeni, zda je parametr cislo
{
    long a;
    char *c;
    a = strtol(argv, &c, 10);
    if (c[0] == '\0' && c[0] != argv[0]) 
    {
        return a;
    }
    return -1;
}
void MapNumber()
{
    MMAP(MoleculeCounter);
    MMAP(ProcessCounter);
    MMAP(BarCounter);
    MMAP(Oxygen);
    MMAP(Hydrogen);
    MMAP(createdCounter);
    MMAP(OxyQueue);
    MMAP(HydroQueue);
    MMAP(Enough);
    
}
void cleanup()
{
    CloseSem();
    UnlinkSem();
    UnmapNumber();
}
void CloseSem()
{
    sem_close(kyslik);
    sem_close(vodik);
    sem_close(Mutex);
    sem_close(barrier2);
    sem_close(barrier);
    sem_close(barrier_mutex);
    sem_close(writing);
    sem_close(Time);
}
void UnlinkSem()
{
    sem_unlink("xstipe02Kyslik");
    sem_unlink("xstipe02Vodik");
    sem_unlink("xstipe02Mutex");
    sem_unlink("xstipe02Barrier");
    sem_unlink("xstipe02Barrier2");
    sem_unlink("xstipe02BarMutex");
    sem_unlink("xstipe02BarMutex");
    sem_unlink("xstipe02Time");
}
void UnmapNumber()
{
    UNMAP(MoleculeCounter);
    UNMAP(ProcessCounter);
    UNMAP(BarCounter);
    UNMAP(Oxygen);
    UNMAP(Hydrogen);
    UNMAP(createdCounter);
    UNMAP(OxyQueue);
    UNMAP(HydroQueue);
    UNMAP(Enough);
    
}
int CheckMap()
{
    if ( MoleculeCounter == MAP_FAILED  || createdCounter == MAP_FAILED || ProcessCounter == MAP_FAILED || BarCounter == MAP_FAILED || Oxygen == MAP_FAILED || Hydrogen == MAP_FAILED)
    {
        fprintf(stderr, "Chyba pri mapovani promennych");
        exit(1);
    }
    return 0;
}
void CheckArguments(int kyslik, int vodik)
{
    if (kyslik == 0 || vodik == 0)
    {
        fprintf(stderr, "Set value at least to 1\n");
        exit(1);
    }
    
}