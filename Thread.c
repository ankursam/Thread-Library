#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <malloc.h>
#include <ucontext.h>
#include "mythread.h"
#define STACK 8*8*1024
#define MAX 256


#define handle_error(msg) \
do
{
  perror(msg);
  exit(EXIT_FAILURE);
} while (0)

ucontext_t main_contxt, new_contxt, temp_contxt, init_contxt;
static int TID = 0,SID= 0;
int active_semaphore=0;

struct pMyThread
{
  ucontext_t contxt;
  int threadID;
  int status; /*1=Ready, 2=Running, 3=Blocked, 4=Terminated,5=Waiting,6=joinall*/
  struct pMyThread *Parent;
  int active_children;
  int is_blocking_parent;/*0=False,1=true*/
  int blocked_by_children;
};

typedef struct pMyThread * _MyThread;
volatile _MyThread current_thread,first_thread;

_MyThread frontelement();
void enqueue(_MyThread thread);
void dequeue();
int isempty();
void create();
void queue_size();
_MyThread frontelements(int a);
void enqueues(_MyThread thread,int a);
void dequeues(int a);
int isemptys(int a);
void creates(int a);
void queue_sizes(int a);

int size=0,sizes[1000]={0};


void MyThreadInit (void(*start_funct)(void *), void *args)
{
  //printf("\n************************BEGIN*************************\n");
  _MyThread init_thread = (_MyThread)malloc(sizeof(struct pMyThread));
  if (getcontext(&main_contxt) == -1)
  handle_error("getcontext");
  getcontext(&main_contxt);
  main_contxt.uc_link = &init_thread->contxt;
  main_contxt.uc_stack.ss_sp = malloc(STACK);
  main_contxt.uc_stack.ss_size = STACK;
  main_contxt.uc_stack.ss_flags = 0;
  makecontext(&main_contxt, (void (*)(void)) start_funct,1,args);
  init_thread->contxt = main_contxt;
  init_thread->threadID = ++TID;
  init_thread->status = 2;
  init_thread->Parent = NULL;
  init_thread->active_children = 0;
  init_thread->is_blocking_parent=0;
  init_thread->blocked_by_children=0;
  create();
  current_thread = init_thread;
  first_thread=init_thread;
  //check for error
  //printf("PID=01 initiated\n");
  //current_thread->status=3;
  getcontext(&init_contxt);
  swapcontext(&init_contxt,&main_contxt);
  //printf("\n~~~~~Thread %d Terminated~~~~~",current_thread->threadID);
  //printf("\n************************THE-END***************************\n");
  //exit(EXIT_SUCCESS);

}

MyThread MyThreadCreate(void(*start_funct)(void *), void *args)
{
  _MyThread new_thread = (_MyThread)malloc(sizeof(struct pMyThread));
  getcontext(&new_contxt);
  new_contxt.uc_link = &current_thread->contxt;
  new_contxt.uc_stack.ss_sp = malloc(STACK);
  new_contxt.uc_stack.ss_size = STACK;
  new_contxt.uc_stack.ss_flags = 0;
  makecontext(&new_contxt, (void (*)(void)) start_funct,1,args);
  new_thread->contxt = new_contxt;
  new_thread->threadID = ++TID;
  new_thread->status = 1;
  new_thread->active_children = 0;
  new_thread->is_blocking_parent=0;
  new_thread->blocked_by_children=0;
  new_thread->Parent = current_thread;
  new_thread->Parent->active_children++;

  /*if(new_thread->Parent->threadID==1)
  {
  current_thread=new_thread;
  new_thread->is_blocking_parent=1;
  //enqueue(new_thread);
  //printf("PID=%d initiated now parent with PID=%d has %d children\n",new_thread->threadID,new_thread->Parent->threadID,new_thread->Parent->active_children);
  return (_MyThread)new_thread;
}*/
//else
{

  //printf("PID=%d initiated now parent with PID=%d has %d children\n",new_thread->threadID,new_thread->Parent->threadID,new_thread->Parent->active_children);

  enqueue(new_thread);
  return (_MyThread)new_thread;
}
}

void MyThreadYield(void)
{
  //Dead Parent Check
  if(current_thread->Parent!=NULL && current_thread->Parent->status==4)
  current_thread->Parent=NULL;
  else{}

  _MyThread temp = (_MyThread)malloc(sizeof(struct pMyThread));
  temp=current_thread;
  if(current_thread->threadID==1 && isempty()==1)
  setcontext(&init_contxt);
  else{
    enqueue(current_thread);
    current_thread->status=1;
    //printf("\nThread %d Yielded and",current_thread->threadID);
    current_thread = frontelement();
    current_thread->status=2;
    //printf("\n#####now Thread %d is running#####",current_thread->threadID);
    dequeue();
    queue_size();
    swapcontext(&temp->contxt,&current_thread->contxt);}
  }

  void MyThreadExit(void)
  {
    //_MyThread temp;
    int check;

    //Dead Parent Check
    if(current_thread->Parent!=NULL && current_thread->Parent->status==4)
    current_thread->Parent=NULL;
    else{}

    current_thread->status=4;
    if(current_thread->Parent==NULL)
    {
      if(current_thread->active_children!=0)
      {
        //printf("\n~~~~~Thread %d Terminated~~~~~",current_thread->threadID);
        current_thread = frontelement();
        current_thread->status=2;
        //printf("\n#####now Thread %d is running#####",current_thread->threadID);
        dequeue();
        queue_size();
        setcontext(&current_thread->contxt);
      }
      else
      setcontext(&init_contxt);
    }
    else
    {
      check=current_thread->Parent->blocked_by_children;
      current_thread->Parent->active_children--;
      if(current_thread->Parent->status==3 && current_thread->is_blocking_parent==1)
      {
        current_thread->Parent->blocked_by_children--;
        current_thread->is_blocking_parent=0;
        enqueue(current_thread->Parent);
        current_thread->Parent->status=1;
        //printf("\n~~~~~Thread %d Terminated~~~~~",current_thread->threadID);
        current_thread = frontelement();
        current_thread->status=2;
        //printf("\n#####now Thread %d is running#####",current_thread->threadID);
        dequeue();
        queue_size();
        setcontext(&current_thread->contxt);
      }
      else if( current_thread->Parent->status==6 )
      {
        current_thread->Parent->blocked_by_children--;
        if(check==1)
        {
          enqueue(current_thread->Parent);
          current_thread->Parent->status=1;
        }
        //printf("\n~~~~~Thread %d Terminated~~~~~",current_thread->threadID);
        current_thread = frontelement();
        current_thread->status=2;
        //printf("\n#####now Thread %d is running#####",current_thread->threadID);
        dequeue();
        queue_size();
        setcontext(&current_thread->contxt);

      }
      else
      {
        //printf("\n~~~~~Thread %d Terminated~~~~~",current_thread->threadID);
        current_thread = frontelement();
        current_thread->status=2;
        //printf("\n#####now Thread %d is running#####",current_thread->threadID);
        dequeue();
        queue_size();
        setcontext(&current_thread->contxt);
      }
    }

  }

  int MyThreadJoin(MyThread thread)
  {
    _MyThread temp;
    _MyThread _thread=(_MyThread)thread;

    //Dead Parent Check
    if(current_thread->Parent!=NULL && current_thread->Parent->status==4)
    current_thread->Parent=NULL;
    else{}

    if(_thread->status==4)
    return 0;

    else if(_thread->Parent!=current_thread)
    return -1;

    else
    {
      current_thread->status=3;
      _thread->is_blocking_parent=1;
      current_thread->blocked_by_children++;
      //printf("\nThread %d is waiting for child thread %d to terminate",current_thread->threadID,_thread->threadID);

      temp=current_thread;//getcontext(&current_thread->contxt);
      current_thread = frontelement();
      //printf("\n#####now Thread %d is running#####",current_thread->threadID);
      /*if(current_thread==NULL)
      {break;}*/
      current_thread->status=2;
      dequeue();
      queue_size();
      swapcontext(&temp->contxt,&current_thread->contxt);

      /*temp=current_thread;//getcontext(&current_thread->contxt);
      current_thread->status=1;
      enqueue(current_thread);
      current_thread = frontelement();
      current_thread->status=2;
      //printf("\nCHECK1");
      dequeue();
      queue_size();
      swapcontext(&temp->contxt,&current_thread->contxt);*/
      return 0;
    }

  }

  void MyThreadJoinAll()
  {
    //Dead Parent Check
    if(current_thread->Parent!=NULL && current_thread->Parent->status==4)
    current_thread->Parent=NULL;
    else{}

    _MyThread temp=(_MyThread)malloc(sizeof(struct pMyThread));;
    current_thread->status=6;
    //printf("\nThread %d is waiting for %d children to terminate",current_thread->threadID,current_thread->active_children);
    temp=current_thread;
    current_thread->blocked_by_children=current_thread->active_children;
    current_thread = frontelement();
    /*if(current_thread==NULL)
    {
    break;
  }*/
  current_thread->status=2;
  //printf("\n#####~~~now Thread %d is running#####",current_thread->threadID);
  dequeue();
  queue_size();
  swapcontext(&temp->contxt,&current_thread->contxt);
}

/*Implementation of a queue of threads using Linked Lists -Reference www.sanfoundry.com */

struct Thread_Queue
{
  _MyThread thread;
  struct Thread_Queue *next;
}*front,*rear,*temp,*front1;


/*Create an empty queue*/
void create()
{
  front = rear = NULL;
}

/* Returns queue size */
void queue_size()
{
  //printf("\nQueue size : %d\n", size);
}

/* Adding thread to the Queue*/
void enqueue(_MyThread thread)
{
  //printf("\nenqueueing thread %d",thread->threadID);
  if (rear == NULL)
  {
    //check null return
    rear = (struct Thread_Queue *)malloc(1*sizeof(struct Thread_Queue));
    rear->next = NULL;
    rear->thread = thread;
    front = rear;
  }
  else
  {
    temp=(struct Thread_Queue *)malloc(1*sizeof(struct Thread_Queue));
    rear->next = temp;
    temp->thread = thread;
    temp->next = NULL;

    rear = temp;
  }
  size++;
}

/* Removing thread from the Queue*/
void dequeue()
{
  front1 = front;
  //printf("\ndequeueing thread %d",front->thread->threadID);

  if (front1 == NULL)
  {
    //printf("\nQueue is Empty");
    return;
  }
  else
  if (front1->next != NULL)
  {
    front1 = front1->next;
    free(front);
    front = front1;
  }
  else
  {
    free(front);
    front = NULL;
    rear = NULL;
  }
  size--;
}

/* Returns the front element of queue */
_MyThread frontelement()
{
  if ((front != NULL) && (rear != NULL)){
    //printf("\nfront element is thread-%d",front->thread->threadID);
    return(front->thread);
  }
  else
  {
    //printf("\nQueue is empty");
    return NULL;
  }
}

/* Display if queue is empty or not */
int isempty()
{
  if ((front == NULL) && (rear == NULL))
  return(1);
  else
  return(0);
}

/*Semaphore Implementation*/

struct Semaphore_Queue
{
  _MyThread sem_thread ;
  struct Semaphore_Queue *next;
}*fronts[1000],*rears[1000],*temps[1000],*fronts1[1000];


struct pMySemaphore
{
  int value;
  int spawned,ID;
  struct Semaphore_Queue *thread_list;
};

typedef struct pMySemaphore * _MySemaphore;


MySemaphore MySemaphoreInit(int initialValue)
{
  _MySemaphore sem = (_MySemaphore)malloc(sizeof(struct pMySemaphore));
  if(sem != NULL)
  {
    sem->value = initialValue;
    sem->ID=++SID;
    creates(sem->ID);
    sem->spawned=1;
    sem->thread_list=fronts[sem->ID];
    active_semaphore++;
  }

  return sem;
}

void MySemaphoreSignal(MySemaphore sem)
{
  _MySemaphore _sem=(_MySemaphore)sem;

  if(active_semaphore<1)
  {
    //printf("\n$$$$$Semaphore not Initialized$$$$$\n");
    exit(EXIT_FAILURE);
  }
  else{}

  _sem->value++;
  if(_sem->value<=0)
  {
    enqueue(frontelements(_sem->ID));
    dequeues(_sem->ID);
    queue_sizes(_sem->ID);
  }
}

void MySemaphoreWait(MySemaphore sem)
{
  _MySemaphore _sem=(_MySemaphore)sem;

  //check
  if(active_semaphore<1)
  {
    //printf("\n$$$$$Semaphore not Initialized$$$$$");
    exit(EXIT_FAILURE);
  }
  else{}

  _sem->value--;
  if(_sem->value<0)
  {
    _MyThread temp = (_MyThread)malloc(sizeof(struct pMyThread));
    temp=current_thread;
    enqueues(current_thread,_sem->ID);
    current_thread->status=5;
    //printf("\nThread %d Waiting for resource and",current_thread->threadID);
    current_thread = frontelement();
    current_thread->status=2;
    //printf("\n#####now Thread %d is running#####",current_thread->threadID);
    dequeue();
    queue_size();
    swapcontext(&temp->contxt,&current_thread->contxt);
  }
}

int MySemaphoreDestroy(MySemaphore sem)
{
  _MySemaphore _sem=(_MySemaphore)sem;

  //check
  if(active_semaphore<1)
  {
    //printf("\n$$$$$Semaphore not Initialized$$$$$");
    exit(EXIT_FAILURE);
  }
  else{}

  if(isemptys(_sem->ID)==1)
  {
    active_semaphore--;
    _sem->spawned=0;
    free(_sem->thread_list);
    return 0;
  }
  else
  return -1;
}


/*Create an empty queue*/
void creates(int i)
{
  fronts[i] = rears[i] = NULL;
}

/* Returns queue size */
void queue_sizes(int i)
{
  //printf("\nSemaphore Queue size : %d\n", sizes[i]);
}

/* Adding thread to the Queue*/
void enqueues(_MyThread thread,int i)
{
  //printf("\nenqueueing thread %d into Semaphore %d Queue ",thread->threadID,i);
  if (rears[i] == NULL)
  {
    //check null return
    rears[i] = (struct Semaphore_Queue *)malloc(1*sizeof(struct Semaphore_Queue));
    rears[i]->next = NULL;
    rears[i]->sem_thread = thread;
    fronts[i] = rears[i];
  }
  else
  {
    temps[i]=(struct Semaphore_Queue *)malloc(1*sizeof(struct Semaphore_Queue));
    rears[i]->next = temps[i];
    temps[i]->sem_thread = thread;
    temps[i]->next = NULL;

    rears[i] = temps[i];
  }
  sizes[i]++;
}

/* Removing thread from the Queue*/
void dequeues(int i)
{
  fronts1[i] = fronts[i];
  //printf("\ndequeueing thread %d from Semaphore queue",fronts[i]->sem_thread->threadID);

  if (fronts1[i] == NULL)
  {
    //printf("\nSemaphore %d Queue is Empty",i);
    return;
  }
  else
  if (fronts1[i]->next != NULL)
  {
    fronts1[i] = fronts1[i]->next;
    free(fronts[i]);
    fronts[i] = fronts1[i];
  }
  else
  {
    free(fronts[i]);
    fronts[i]= NULL;
    rears[i] = NULL;
  }
  sizes[i]--;
}

/* Returns the front element of queue */
_MyThread frontelements(int i)
{
  if ((fronts[i] != NULL) && (rears[i] != NULL)){
    //printf("\nfront element from Semaphore %d Queue is thread-%d",i,fronts[i]->sem_thread->threadID);
    return(fronts[i]->sem_thread);
  }
  else
  {
    //printf("\nSemaphore %d Queue is empty",i);
    return NULL;
  }
}

/* Display if queue is empty or not */
int isemptys(int i)
{
  if ((fronts[i] == NULL) && (rears[i] == NULL))
  return(1);
  else
  return(0);
}
