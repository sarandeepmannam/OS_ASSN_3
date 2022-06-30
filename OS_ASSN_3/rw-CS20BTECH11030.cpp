#include <iostream>
#include <algorithm>
#include <semaphore.h>
#include <sys/time.h>
#include <limits.h>
#include <unistd.h>
#include <fstream>
#include <random>
#include <cstdlib>
#include <pthread.h>
#include <chrono>
#include <ctime>

using namespace std;

int nw ,nr; //no of writer,reader threads respectively
int kw,kr;//frequency of writer,reader threads respectively
float mu_cs,mu_rem ;//Average time in CS and Remainder section respectively
int read_count = 0;
FILE* f; 
FILE* f2;
default_random_engine gen1 , gen2 ;
exponential_distribution<double> cs;
exponential_distribution<double> rem; 
// semaphores 
sem_t mutex , rw_mutex ;
struct Time
{
  int hr;
  int min;
  int sec;
};
typedef struct Time time_x;
time_x getSysTime()//Function to get the time
{  
  time_x x;
  time_t rawtime;
  struct tm * timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  x.hr=timeinfo->tm_hour;
  x.min=timeinfo->tm_min;
  x.sec=timeinfo->tm_sec;
  return x;
}

double reader_tot_time = 0 , writer_tot_time = 0 , reader_max_time =0 , writer_max_time = 0; // metrics

// readers code
void *reader(void *p)
{
	int tid = *((int*)p); // reader thread ID
	for(int i=0;i<kr;i++)
  {
    time_x time;
    time=getSysTime();
    auto start=std::chrono::system_clock::now();
		// entry section
		fprintf(f," %d CS Request by reader thread %d at %d:%d:%d\n",i+1,tid,time.hr,time.min,time.sec);
    
		sem_wait(&mutex);
		read_count++;
		if (read_count == 1)
    {// block all writers
			sem_wait(&rw_mutex);
    }
		sem_post(&mutex);
    time_x time1;
    time1=getSysTime();
    auto end=std::chrono::system_clock::now();
		//double wait =  (time1.hr-time.hr)*3600+(time1.min-time.min)*60+(time1.sec-time.sec);
    std::chrono::duration<double> elapsed_seconds = end-start;

		reader_tot_time += elapsed_seconds.count();
    reader_max_time = max(reader_max_time,elapsed_seconds.count()*1e3);
    
		// critical section
		fprintf(f," %d CS Entry by Reader Thread %d at %d:%d:%d \n" , i+1 , tid , time1.hr,time1.min,time1.sec);
		gen1.seed(tid*nr + i);
		usleep(cs(gen1)*1e3); // execution in critical section
		sem_wait(&mutex);
		read_count--;
		if (read_count == 0) // allow writers to execute
    {
      sem_post(&rw_mutex);
    }
		sem_post(&mutex);

    time_x time2;
    time2=getSysTime();
		// remainder section
		fprintf(f," %d CS Exit by Reader Thread %d at %d:%d:%d\n" ,i+1,tid,time2.hr,time2.min,time2.sec);
		usleep(rem(gen2)*1e3); // execution in remainder section
	}
  pthread_exit(0);
}

// writers code
void *writer(void *p)
{
	int tid = *((int*)p); // writer thread ID
	for(int i=0;i<kw;i++){
    time_x time;
    time=getSysTime();
    auto start=std::chrono::system_clock::now();
		// entry section
		fprintf(f," %d CS Request by Writer Thread %d at %d:%d:%d \n" ,i+1,tid,time.hr,time.min,time.sec );
		sem_wait(&rw_mutex);
   time_x time1;
    time1=getSysTime();
    auto end=std::chrono::system_clock::now();
		//double wait = (time1.hr-time.hr)*3600+(time1.min-time.min)*60+(time1.sec-time.sec);
		std::chrono::duration<double> elapsed_seconds = end-start;

		writer_tot_time += elapsed_seconds.count();
    writer_max_time = max(writer_max_time,elapsed_seconds.count()*1e3);

		// critical section
		fprintf(f," %d CS Entry by Writer Thread %d at %d:%d:%d \n" , i+1,tid,time1.hr,time1.min,time1.sec);

		usleep(cs(gen1)*1e3); // stimulate critical section

		sem_post(&rw_mutex);

		time_x time2;
    time2=getSysTime();
		// exit section 
		fprintf(f," %d CS Exit by Writer Thread %d at %d:%d:%d\n" , i+1 , tid ,time2.hr,time2.min,time2.sec);
		usleep(rem(gen2)*1e3); // stimulate remainder section
	}
   pthread_exit(0);
}

int main(){

  FILE* f1;
  f1=fopen("inp-params.txt","r");
  if(f1!=NULL)
  {
    cout<<"File opening was successful\n";
  }
  else
  {
    cout<<"File opening was unsuccessful\n";
  }
  fscanf(f1,"%d %d %d %d %f %f",&nw,&nr,&kw,&kr,&mu_cs,&mu_rem);
  fclose(f1);
  rem = exponential_distribution<double>(1/mu_rem);
	cs = exponential_distribution<double>(1/mu_cs);
	// writer threads
	pthread_t writers[nw];
	int writer_id[nw];
	pthread_attr_t writer_attr[nw];

	// reader threads
	pthread_t readers[nr];
	int reader_id[nr];
	pthread_attr_t reader_attr[nr];

	// semaphores initialisations
	sem_init(&mutex, 0, 1);
	sem_init(&rw_mutex, 0, 1);
  f=fopen("RW-log.txt","w");
  f2=fopen("Average_time.txt","w");
	// spawn writer threads
	for(int i=0;i<nw;i++){ 
		writer_id[i] = i+1;
		pthread_attr_init(&writer_attr[i]);
		pthread_create(&writers[i] ,&writer_attr[i],writer,&writer_id[i]);
	}

	// spawn readers threads
	for(int i=0;i<nr;i++){ 
		reader_id[i] = i+1;
		pthread_attr_init(&reader_attr[i]);
		pthread_create(&readers[i] ,&reader_attr[i],reader,&reader_id[i]);
	}

	// wait till all threads are finished
	for(int i=0;i<nw;i++){ 
		pthread_join(writers[i],NULL);
	}

	for(int i=0;i<nr;i++){ 
		pthread_join(readers[i],NULL);
	}

	fprintf(f2,"Average time in standard reader-wrters is %lf milliseconds\n" , (writer_tot_time+ reader_tot_time)/(nr*kr*1e-3 + nw*kw*1e-3));
	cout<<"Average waiting time of reader:"<<reader_tot_time/(nr*kr*1e-3)<<endl;
	cout<<"Average waiting time of writer:"<<writer_tot_time/(nw*kw*1e-3)<<endl;
	cout<<"Worst case waiting time of reader:"<<reader_max_time<<endl;
  cout<<"Worst case waiting time of writer:"<<writer_max_time<<endl;
  cout<<nr*kr*1e-3;
	return 0;
}