/******************************************************************
 * The Main program with the two functions. A simple
 * example of creating and using a thread is provided.
 ******************************************************************/

#include "helper.hpp"
// #include <semaphore.h>
#define INCORRECT_NUMBER_OF_PARAMETERS 1
#define INVALID_PARAMETER 2
#define SEMAPHORE_ERROR 3

Program program; // Stores all values for the program, so that it can be accessed by all functions
Job** jobQueue;
void *producer (void *id);
void *consumer (void *id);
int producerIndex;
int consumerIndex;

// sem_t semEmpty; // Stores emptiness of the queue
// sem_t semFull; // Stores fullness of the queue

int main (int argc, char **argv)
{
  producerIndex = -1;
  consumerIndex = -1;
  // ============================== INITIALIZE ARGUMENTS =====================================
  program.queueSize = check_arg(argv[1]);
  program.numberOfJobs = check_arg(argv[2]);
  program.numberProducers = check_arg(argv[3]);
  program.numberConsumers = check_arg(argv[4]);

  // ========================== ERROR MESSAGES FROM COMMAND LINE =============================
  if (argc != 5) {
    cerr << "Please enter 4 integers after './main'." << endl;
    return INCORRECT_NUMBER_OF_PARAMETERS;
  }
  if (program.queueSize < 0 || program.numberOfJobs < 0 || program.numberProducers < 0 || program.numberConsumers < 0) {
    cerr << "Please only enter positive integers." << endl;
    return INVALID_PARAMETER;
  }

  cout << "Queue size: \t" << program.queueSize << "\n"
  "Jobs per producer: \t" << program.numberOfJobs << "\n"
  "Number of producers: \t" << program.numberProducers << "\n"
  "Number of consumers: \t" << program.numberConsumers << "\n";

  // ================================ CREATE SEMAPHORES ======================================
  program.semID = sem_create(SEM_KEY, 3); // create semaphores
  /* Here 3 semaphores created:
      Semaphore 0: Mutex semaphore
      Semaphore 1: Stores fullness of buffer
      Semaphore 2: Stores emptiness of buffer */

  if (program.semID < 0) {
    cerr << "Error creating sempahore set." << endl;
    return SEMAPHORE_ERROR;
  }
  // ----- Initialize semaphores -----
  if (sem_init(program.semID, 0, 1) < 0) {
    cerr << "Error creating mutex semaphore." << endl;
    return SEMAPHORE_ERROR;
  } // mutex
  if (sem_init(program.semID, 1, program.queueSize) < 0) {
    cerr << "Error creating semaphore 1." << endl;
    return SEMAPHORE_ERROR;
  } // empty (queueSize)
  if (sem_init(program.semID, 2, 0) < 0) {
    cerr << "Error creating semaphore 2." << endl;
    return SEMAPHORE_ERROR;
  } // fullness (nothing there yet)

  // sem_init(&semEmpty, 0, queueSize); // create empty buffer with queueSize number of spaces
  // sem_init(&semFull, 0, 0); // nothing in full

  // ================================== THREAD CREATION =====================================
  // create arrays for the right amount of threads for producers and consumers
  pthread_t producerid[program.numberProducers];
  pthread_t consumerid[program.numberConsumers];

  jobQueue = new Job*[program.queueSize];
  int parameter[9];
  for (int i = 0; i < 10; i++) {
    parameter[i] = i+1;
  }

  // Create producer threads
  for (int i = 0; i < program.numberProducers; i++) {
    if (pthread_create (&producerid[i], NULL, &producer, (void *) &parameter[i]) != 0)
      perror("Failed to create thread");
  }
  // Create consumer threads
  for (int i = 0; i < program.numberConsumers; i++) {
    if (pthread_create (&consumerid[i], NULL, &consumer, (void *) &parameter[i]) != 0)
      perror("Failed to create thread");
  }

  // ==================================== THREAD JOINS ======================================
  // Join producer threads
  for (int i = 0; i < program.numberProducers; i++) {
    if (pthread_join(producerid[i], NULL) != 0) {
      perror("Failed to join thread");
    }
  }
  // Join consumer threads
  for (int i = 0; i < program.numberConsumers; i++) {
    if (pthread_join(consumerid[i], NULL) != 0) {
      perror("Failed to join thread");
    }
  }

  cout << "Doing some work after the join" << endl;

  sem_close(program.semID);
  // sem_destroy(&semEmpty);
  // sem_destroy(&semFull);
  return 0;
}

void *producer (void *parameter) 
{
  int *param = (int *) parameter;
  for (int i = 0; i < program.numberOfJobs; i++) {
    // Check emptiness semaphore, if wait exceeds 20 seconds then end.
    if (sem_timed_wait(program.semID, 1, 20) < 0) {
      cerr << "Producer(" << *param << ") has waited for over 20 seconds! >:(" << endl;
      pthread_exit(0);
    }
    // Do a job that takes 1-5 seconds
    sleep(rand()%5 + 1); // sleeps for a random amount of time between 1-5 seconds
    
    // ==================== CRITICAL START ===================
    sem_wait(program.semID, 0);

    // Circular queue for producer pointer
    do {
      producerIndex = (producerIndex+1) % (program.numberOfJobs-1);
    } while (jobQueue[producerIndex] != NULL); // cycyles the queue to find an empty spot
    
    jobQueue[producerIndex] = new Job((producerIndex+1), (rand()%10+1)); // create new job with random duration between 1-10 seconds
    cout << "Producer(" << *param << "): Job ID " << jobQueue[producerIndex]->jobID << 
    " duration " << jobQueue[producerIndex]->duration << endl;
    sem_signal(program.semID,0);
    sem_signal(program.semID,2);
    // ===================== CRITICAL END =====================
  }
  cout << "Producer(" << *param << "): No more jobs to generate." << endl;

  pthread_exit(0);
}

void *consumer (void *parameter) 
{
  int *param = (int *) parameter;
  int sleepDur = 0;
  
  while (true) {
    if (sem_timed_wait(program.semID, 2, 20) < 0) {
      cerr << "Consumer(" << *param << "): No more jobs left." << endl;
      pthread_exit(0);
    }
    // ==================== CRITICAL START ===================
    sem_wait(program.semID, 0);
    // Circular queue for consumer pointer
    do {
      consumerIndex = (consumerIndex+1) % (program.numberOfJobs-1);
    } while (jobQueue[consumerIndex] == NULL);

    cout << "Consumer(" << *param << ") Job ID " << jobQueue[consumerIndex]->jobID
    << " executing sleep duration " << jobQueue[consumerIndex]->duration << endl;
    sleepDur = jobQueue[consumerIndex]->duration;
    // Consume Job
    delete jobQueue[consumerIndex];
    jobQueue[consumerIndex] = NULL;
    int temp = consumerIndex; 
    sem_signal(program.semID, 0);
    sem_signal(program.semID, 1);
    // ===================== CRITICAL END =====================

    sleep(sleepDur);
    cout << "Consumer (" << *param << ") Job ID " << temp+1 << " completed." << endl;
    // pthread_exit (0);
  }
}
