/* Tests cetegorical mutual exclusion with different numbers of threads.
 * Automatic checks only catch severe problems like crashes.
 */
#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "lib/random.h" //generate random numbers

#define BUS_CAPACITY 3
#define SENDER 0
#define RECEIVER 1
#define NORMAL 0
#define HIGH 1

/*
 *  initialize task with direction and priority
 *  call o
 * */
typedef struct {
  int direction;
  int priority;
} task_t;
void init_bus(void);
void batchScheduler(unsigned int num_tasks_send, unsigned int num_task_receive,
        unsigned int num_priority_send, unsigned int num_priority_receive);

void senderTask(void *);
void receiverTask(void *);
void senderPriorityTask(void *);
void receiverPriorityTask(void *);


void oneTask(task_t task);/*Task requires to use the bus and executes methods below*/
void getSlot(task_t task); /* task tries to use slot on the bus */
void transferData(task_t task); /* task processes data on the bus either sending or receiving based on the direction*/
void leaveSlot(task_t task); /* task release the slot */


/*current direction of data transfer*/
int curr_direction = SENDER;  
/**global semaphores*/
struct semaphore bus_slots;  /*for BUS_CAPACITY slots*/
struct semaphore send_pri_task_sema; /*priority tasks waiting to send*/
struct semaphore rcv_pri_task_sema; /*priority tasks waiting to recieve*/
struct semaphore send_norm_task_sema; /*tasks waiting to send*/
struct semaphore rcv_norm_task_sema; /*tasks waiting to receive*/

struct lock bus_mutex;   /*ensures mutual exclusion*/

/* initializes semaphores */ 
void init_bus(void){ 
 
    random_init((unsigned int)123456789); 

    sema_init(&bus_slots,BUS_CAPACITY);  /*bus has BUS_CAPACITY slots*/
    
    /*counting sema*/
    sema_init(&send_pri_task_sema,0);
    sema_init(&rcv_pri_task_sema,0);
    sema_init(&send_norm_task_sema,0);
    sema_init(&rcv_norm_task_sema,0);
    lock_init (&bus_mutex);

}

/*
 *  Creates a memory bus sub-system  with num_tasks_send + num_priority_send
 *  sending data to the accelerator and num_task_receive + num_priority_receive tasks
 *  reading data/results from the accelerator.
 *
 *  Every task is represented by its own thread. 
 *  Task requires and gets slot on bus system (1)
 *  process data and the bus (2)
 *  Leave the bus (3).
 */

void batchScheduler(unsigned int num_tasks_send, unsigned int num_task_receive,
        unsigned int num_priority_send, unsigned int num_priority_receive)
{

    unsigned int i;

    /*batchScheduler receives tasks and iteratively passes them to the function 
    handlers deepending on direction and priority*/   

    for(i=0; i < num_tasks_send; i++)
        thread_create("tsk_send", 1, senderTask, NULL );

    for(i=0; i < num_priority_send; i++)
        thread_create("tsk_priority_send", 1, senderPriorityTask, NULL );

    for(i=0; i < num_task_receive; i++)
        thread_create("tsk_receive", 1, receiverTask, NULL );

    for(i=0; i < num_priority_receive; i++)
        thread_create("tsk_priority_receive", 1, receiverPriorityTask, NULL );
}

/* Normal task,  sending data to the accelerator */
void senderTask(void *aux UNUSED){
        task_t task = {SENDER, NORMAL};
        oneTask(task);
}

/* High priority task, sending data to the accelerator */
void senderPriorityTask(void *aux UNUSED){
        task_t task = {SENDER, HIGH};
        oneTask(task);
}

/* Normal task, reading data from the accelerator */
void receiverTask(void *aux UNUSED){
        task_t task = {RECEIVER, NORMAL};
        oneTask(task);
}

/* High priority task, reading data from the accelerator */
void receiverPriorityTask(void *aux UNUSED){
        task_t task = {RECEIVER, HIGH};
        oneTask(task);
}

/* abstract task execution*/
void oneTask(task_t task) {
 getSlot(task);
  transferData(task);
 leaveSlot(task);

}


/* task tries to get slot on the bus subsystem */
void getSlot(task_t task) 
{
   
    /*check priority, then direction and increase the respective semapores */
    if(task.priority == HIGH) 
      {
      if(task.direction == SENDER)
          sema_up(&send_pri_task_sema);
      else 
          sema_up(&rcv_pri_task_sema); 
      } 
    
    else  
      {      /*for normal tasks*/
      if(task.direction == SENDER)     
          sema_up(&send_norm_task_sema);  
      else 
          sema_up(&rcv_norm_task_sema);
      }       

/*if the direction is differnt from mine*/
    while(true)
       {
/*enter critical section*/
lock_acquire(&bus_mutex);
        /*checks if bus is free or in our direction*/
        if(bus_slots.value == BUS_CAPACITY || curr_direction ==  task.direction )    
         {  

            /*If high priority or no high priority is waiting*/
           if(task.priority == HIGH || (send_pri_task_sema.value==0 && rcv_pri_task_sema.value==0))
             {
               /*change to my direction for the case where the bus was free*/
              curr_direction = task.direction; 

              /*reduce free slots on the bus*/
              sema_down(&bus_slots);

              if(task.priority == HIGH) {
                if(task.direction == SENDER)
                  sema_down(&send_pri_task_sema);
                else 
                  sema_down(&rcv_pri_task_sema); 
                } 
              
              else  {    /*for normal tasks*/
                if(task.direction == SENDER)     
                  sema_down(&send_norm_task_sema);  
                else 
                  sema_down(&rcv_norm_task_sema);
              } 


              lock_release(&bus_mutex);

              break;   /*breaks out of the while loop*/

            }
          }

        /*exit the critical section*/
        
        lock_release(&bus_mutex);

        //give chance to other threads
        thread_yield();

       }
}

/* task processes data on the bus send/receive */
void transferData(task_t task UNUSED) 
{
//msg("Start Tranfer Data");

/*generate random sleeping time between 0 and 10*/
unsigned int random= (unsigned int)random_ulong();
random= random % 10;
timer_sleep((int64_t)random);

//msg("data transfered");
}

/* task releases the slot */
void leaveSlot(task_t task UNUSED) 
{
    /*increament available bus slots*/
    sema_up(&bus_slots);
}
