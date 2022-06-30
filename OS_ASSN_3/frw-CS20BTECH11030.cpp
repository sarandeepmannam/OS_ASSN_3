#include <pthread.h>
#include <iostream>
#include <unistd.h>
#include <fstream>
#include <random>
#include <limits.h>
#include <cstdlib>
#include <semaphore.h>
#include<sys/time.h>
#include <chrono>
#include <ctime>

using namespace std;

// input parameters
int nw ,nr; //no of writer,reader threads respectively
int kw,kr;//frequency of writer,reader threads respectively
float mu_cs,mu_rem ;//Average time in CS and Remainder section respectively
int read_count = 0;
FILE* f; 
FILE* f2;

default_random_engine gen1 , gen2 ;
exponential_distribution<double> rem;
exponential_distribution<double> cs;
// semaphores 
sem_t mutex , rw_mutex , queue;

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

double reader_tot_time = 0 , writer_tot_time = 0 , reader_max_time =0 , writer_max_time = 0;

// readers code
void *reader(void *param){
	int tid = *(int*)param; // reader thread ID

	for(int i=0;i<kr;i++)
  {
		time_x time;
    time=getSysTime();
		// entry section
		fprintf(f," %d CS Request by reader thread %d at %d:%d:%d\n",i+1,tid,time.hr,time.min,time.sec);
    auto start=std::chrono::system_clock::now();

		sem_wait(&queue); // block other threads 
		sem_wait(&mutex);
		read_count++;
		if (read_count == 1) // block writers thread 
			sem_wait(&rw_mutex);
		sem_post(&queue); // release semaphore
		sem_post(&mutex);

		time_x time1;
    time1=getSysTime();
    auto end=std::chrono::system_clock::now();
		std::chrono::duration<double> elapsed_seconds = end-start;

		reader_tot_time += elapsed_seconds.count();
    reader_max_time = max(reader_max_time,elapsed_seconds.count()*1e3);
		// critical section
		fprintf(f," %d CS Entry by Reader Thread %d at %d:%d:%d \n" , i+1 , tid , time1.hr,time1.min,time1.sec);

		usleep(cs(gen1)*1e3); // stimulating critical section
		sem_wait(&mutex);
		read_count--;
		if (read_count == 0)
			sem_post(&rw_mutex);
		sem_post(&mutex);

		time_x time2;
    time2=getSysTime();
		// remainder section
		fprintf(f," %d CS Exit by Reader Thread %d at %d:%d:%d\n" ,i+1,tid,time2.hr,time2.min,time2.sec);
		// exit section
		usleep(rem(gen2)*1e3); // stimulate remainder section.
	}
  pthread_exit(0);
}

// writers code
void *writer(void *param){
	int tid = *(int*)param; // writer thread ID

	for(int i=0;i<kw;i++){

		time_x time;
    time=getSysTime();
    auto start=std::chrono::system_clock::now();
		// entry section
		fprintf(f," %d CS Request by Writer Thread %d at %d:%d:%d \n" ,i+1,tid,time.hr,time.min,time.sec );
		
		sem_wait(&queue); // block queue
		sem_wait(&rw_mutex); // block readers and writers
		sem_post(&queue);

		time_x time1;
    time1=getSysTime();
    auto end=std::chrono::system_clock::now();

		//double wait = (enterTime.tv_sec - reqTime.tv_sec)*1e6 + (enterTime.tv_usec - reqTime.tv_usec);
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
		usleep(rem(gen2)*1e3); // stimulate remainder section.
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
	cs = exponential_distribution<double>(1/mu_cs);
	rem = exponential_distribution<double>(1/mu_rem);
	// writer threads
	pthread_t writer_tid[nw];
	int writer_ids[nw];
	pthread_attr_t writer_attr[nw];

	// reader threads
	pthread_t reader_tid[nr];
	int reader_ids[nr];
	pthread_attr_t reader_attr[nr];

	// semaphores initialisation
  sem_init(&rw_mutex, 0, 1);
	sem_init(&queue,0,1);
	sem_init(&mutex, 0, 1);
	
  f=fopen("FairRW-log.txt","w");
  f2=fopen("Average_time.txt","w");
	// spawn writer threads
	for(int i=0;i<nw;i++){ 
		writer_ids[i] = i+1;
		pthread_attr_init(&writer_attr[i]);
		pthread_create(&writer_tid[i] ,&writer_attr[i],writer,&writer_ids[i]);
	}

	// creating readers threads
	for(int i=0;i<nr;i++){ 
		reader_ids[i] = i+1;
		pthread_attr_init(&reader_attr[i]);
		pthread_create(&reader_tid[i] ,&reader_attr[i],reader,&reader_ids[i]);
	}

	// wait till all writer threads are finished
	for(int i=0;i<nw;i++){ 
		pthread_join(writer_tid[i],NULL);
	}
  // wait till all reader threads are finished
	for(int i=0;i<nr;i++){ 
		pthread_join(reader_tid[i],NULL);
	}
	
	fprintf(f2,"Average time in fair reader-wrters is %lf miliseconds\n" , (writer_tot_time + reader_tot_time)/(nr*kr*1e-3+ nw*kw*1e-3));
	cout<<"Average waiting time of reader:"<<reader_tot_time/(nr*kr*1e-3)<<endl;
	cout<<"Average waiting time of writer:"<<writer_tot_time/(nw*kw*1e-3)<<endl;
	cout<<"Worst case waiting time of reader:"<<reader_max_time<<endl;
  cout<<"Worst case waiting time of writer:"<<writer_max_time<<endl;
	return 0;
}