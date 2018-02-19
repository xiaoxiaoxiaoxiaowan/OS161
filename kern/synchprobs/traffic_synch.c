#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/*
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/*
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
static struct lock *intersection_lock;

int could_access[4][4];

struct cv *intersection_cv[4];

int which_direction(Direction which);

bool if_right_turning(int or, int des);

/*
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 *
 */
void
intersection_sync_init(void)
{
  const char* lock_name = "interlock";
  intersection_lock = lock_create(lock_name);
  if(intersection_lock == NULL){
    panic("Intersection Lock Failed");
  }
  for(int a = 0;a < 4;++a){
    const char* cv_name = "inter_cv";
    intersection_cv[a] = cv_create(cv_name);
    if(intersection_cv == NULL){
      panic("Intersection CV Failed");
    }
  }
  for(int a = 0;a < 4;++a){
    for(int b = 0;b < 4;++b){
      could_access[a][b] = 0;
    }
  }
}

/*
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
   void
intersection_sync_cleanup(void)
{
  KASSERT(intersection_lock != NULL);
  for(int a = 0;a < 4;++a){
    cv_destroy(intersection_cv[a]);
  }
  lock_destroy(intersection_lock);
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

int which_direction(Direction which){
  if(which == north)return 0;
  if(which == east)return 1;
  if(which == south)return 2;
  if(which == west)return 3;
  return 4;
}

bool if_right_turning(int or, int des){
  if(or == 2 && des == 1)return true;
  if(or == 1 && des == 0)return true;
  if(or == 0 && des == 3)return true;
  if(or == 3 && des == 2)return true;
  return false;
}


void
intersection_before_entry(Direction origin, Direction destination) 
{
  KASSERT(intersection_lock != NULL);
  lock_acquire(intersection_lock);
  int start = which_direction(origin);
  int finish = which_direction(destination);
  while(could_access[origin][destination] != 0){
    cv_wait(intersection_cv[origin], intersection_lock);
  }
  for(int a = 0;a < 4;++a){
    for(int b = 0;b < 4;++b){
      if(a == b)continue;
      if(a == start)continue;
      if(a == finish && b == start)continue;
      if(b != finish && (if_right_turning(start, finish) || if_right_turning(a, b)))continue;
      could_access[a][b]++;
    }
  }
  lock_release(intersection_lock);
}




/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
  KASSERT(intersection_lock != NULL);
  lock_acquire(intersection_lock);
  int start = which_direction(origin);
  int finish = which_direction(destination);
  for(int a = 0;a < 4;++a){
    for(int b = 0;b < 4;++b){
      if(a == b)continue;
      if(a == start)continue;
      if(a == finish && b == start)continue;
      if(b != finish && (if_right_turning(start, finish) || if_right_turning(a, b)))continue;
      could_access[a][b]--;
      if(could_access[a][b] == 0){
        cv_broadcast(intersection_cv[a], intersection_lock);
      }
    }
  }
  lock_release(intersection_lock);
}
