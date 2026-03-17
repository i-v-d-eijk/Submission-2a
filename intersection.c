#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#include "arrivals.h"
#include "intersection_time.h"
#include "input.h"

/* 
 * curr_arrivals[][][]
 *
 * A 3D array that stores the arrivals that have occurred
 * The first two indices determine the entry lane: first index is Side, second index is Direction
 * curr_arrivals[s][d] returns an array of all arrivals for the entry lane on side s for direction d,
 *   ordered in the same order as they arrived
 */
static Arrival curr_arrivals[4][3][20];

/*
 * semaphores[][]
 *
 * A 2D array that defines a semaphore for each entry lane,
 *   which are used to signal the corresponding traffic light that a car has arrived
 * The two indices determine the entry lane: first index is Side, second index is Direction
 */
static sem_t semaphores[4][3];

/*
 * supply_arrivals()
 *
 * A function for supplying arrivals to the intersection
 * This should be executed by a separate thread
 */
static void* supply_arrivals()
{
  int num_curr_arrivals[4][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}};

  // for every arrival in the list
  for (int i = 0; i < sizeof(input_arrivals)/sizeof(Arrival); i++)
  {
    // get the next arrival in the list
    Arrival arrival = input_arrivals[i];
    // wait until this arrival is supposed to arrive
    sleep_until_arrival(arrival.time);
    // store the new arrival in curr_arrivals
    curr_arrivals[arrival.side][arrival.direction][num_curr_arrivals[arrival.side][arrival.direction]] = arrival;
    num_curr_arrivals[arrival.side][arrival.direction] += 1;
    // increment the semaphore for the traffic light that the arrival is for
    sem_post(&semaphores[arrival.side][arrival.direction]);
  }

  return(0);
}

/*
* We want to create a global mutex to enfore that only one traffic light can turn green at the same time
*/
static pthread_mutex_t intersection_mutex;

/*
* We use the following code to ensure that each thread know which side it is on and which direction
*/
typedef struct { int side; int direction; } LightArgs;


/*
 * manage_light(void* arg)
 *
 * A function that implements the behaviour of a traffic light
 */
static void* manage_light(void* arg)
{
  // TODO:
  // while it is not END_TIME yet, repeatedly:
  //  - wait for an arrival using the semaphore for this traffic light
  //  - lock the right mutex(es)
  //  - make the traffic light turn green
  //  - sleep for CROSS_TIME seconds
  //  - make the traffic light turn red
  //  - unlock the right mutex(es)
  LightArgs* args = (LightArgs*) arg; 
  int side = args->side;
  int direction = args->direction;

  int car_index = 0;

  while (1)
  {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (END_TIME - get_time_passed());

    if (sem_timedwait(&semaphores[side][direction], &ts) == -1)
    {
      break;
    }

    pthread_mutex_lock(&intersection_mutex);

    Arrival arrival = curr_arrivals[side][direction][car_index]; 
    car_index++;

    printf("traffic light %d %d turns green at time %d for car %d\n", side, direction, get_time_passed(), arrival.id);

    sleep(CROSS_TIME); 
    
    printf("traffic light %d %d turns red at time %d\n", side, direction, get_time_passed());

    pthread_mutex_unlock(&intersection_mutex);
  }
  return(0);
}


int main(int argc, char * argv[])
{
  // create semaphores to wait/signal for arrivals
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      sem_init(&semaphores[i][j], 0, 0);
    }
  }

  //Initialse the mutex
  pthread_mutex_init(&intersection_mutex, NULL);

  // start the timer
  start_time();

  // create a thread per traffic light that executes manage_light
  pthread_t light_threads[4][3]; 
  LightArgs args[4][3];

  for (int i = 0; i < 4; i++) 
  { 
    for (int j = 0; j < 3; j++) 
    { 
      args[i][j].side = i; 
      args[i][j].direction = j; 
      pthread_create(&light_threads[i][j], NULL, manage_light, &args[i][j]); 
    }
  }

  //create a thread that executes supply_arrivals
  pthread_t arrival_thread; 
  pthread_create(&arrival_thread, NULL, supply_arrivals, NULL);
  pthread_join(arrival_thread, NULL);

  // wait for all threads to finish
  for (int i = 0; i < 4; i++) 
  {
    for (int j = 0; j < 3; j++) 
    { 
      pthread_join(light_threads[i][j], NULL); 
    }
  }

  // destroy semaphores
  pthread_mutex_destroy(&intersection_mutex);

  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      sem_destroy(&semaphores[i][j]);
    }
  }
}
