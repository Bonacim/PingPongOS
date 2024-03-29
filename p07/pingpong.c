#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include "pingpong.h"
#include "datatypes.h"
#include "queue.h"

//#define DEBUG
#define STACKSIZE 32768
#define _XOPEN_SOURCE 600
#define TICKS 20

task_t mainTask, dispatcherTask, *currentTask, *ready_queue, *suspend_queue;
int newTaskID, userTasks;

struct sigaction action;
struct itimerval timer;
unsigned int sys_time;

void dispatcher_body();
task_t *scheduler();
void tick_handler(int signum);
void set_timer(int value_sec, int value_usec, int interval_sec, int interval_usec);

void pingpong_init ()
{
    setvbuf (stdout, 0, _IONBF, 0);

    sys_time = 0;
    newTaskID = 0;
    userTasks = 0;

    ready_queue = NULL;
    suspend_queue = NULL;

    task_create(&mainTask, NULL, "Main Task");

    currentTask = &mainTask;

    task_create(&dispatcherTask, dispatcher_body, "Dispatcher");

    set_timer(0, 1000, 0, 1000);

    task_yield();
}

int task_create (task_t *task, void (*start_func)(void *), void *arg)
{
    char *stack ;
    stack = malloc (STACKSIZE) ;

    if (stack)
    {
        task->next = NULL;
        task->prev = NULL;
        task->wqueue = NULL;

        task->tid = newTaskID++;
        task->created_at = sys_time;
		task->proc_time = 0;
		task->activations = 0;

        getcontext(&(task->context));
        task->context.uc_stack.ss_sp = stack;
        task->context.uc_stack.ss_size = STACKSIZE;
        task->context.uc_stack.ss_flags = 0;
        task->context.uc_link = 0;

        if (task != &mainTask)
        {
            makecontext (&(task->context), (void *)(*start_func), 1, arg);
        }

        #ifdef DEBUG
        printf ("task_create: criou tarefa %d\n", task->tid);
        #endif

        if (task != &dispatcherTask)
        {
            queue_append((queue_t**)&ready_queue, (queue_t*)task);
            task->wqueue = ready_queue;
            task->status = pronta;
            task_setprio(task, 0);
            userTasks++;
        }

        return task->tid;
    }

    else
    {
        printf("Erro na criação da pilha!");
        return -1;
    }
}

int task_switch (task_t *task)
{
    task_t *aux = currentTask;
    currentTask = task;
    currentTask->activations++;
    #ifdef DEBUG
    printf ("task_switch: trocando contexto %d -> %d\n", aux->tid, task->tid);
    #endif

    if (swapcontext (&(aux->context), &(task->context)) < 0)
    {
        currentTask = aux;
        return -1;
    }

    return 0;
}

void task_exit (int exitCode)
{
    #ifdef DEBUG
    printf ("task_exit: tarefa %d sendo encerrada\n", currentTask->tid);
    #endif
    int exec_time = sys_time - currentTask->created_at;
    printf("Task %d exit: execution time %d ms, processor time %d ms, %d activations\n", currentTask->tid, exec_time, currentTask->proc_time, currentTask->activations);

    if (currentTask == &dispatcherTask)
    {
        exit(0);
    }

    queue_remove((queue_t**)&ready_queue, (queue_t*)currentTask);
    userTasks--;

    task_switch(&dispatcherTask);
}

int task_id ()
{
    return currentTask->tid;
}

void task_yield ()
{
    task_switch(&dispatcherTask);
}

void dispatcher_body ()
{
    task_t *next;

    while (userTasks > 0)
    {
        next = scheduler();

        if (next)
        {
            next->ticks_t = TICKS;
            task_switch (next);
        }
    }
    task_exit(0);
}

task_t *scheduler()
{
	task_t* aux = ready_queue;
    task_t* next = aux;

	while(aux != NULL && aux->next != ready_queue)
	{
		aux = aux->next;

		if(aux->prio_d < next->prio_d)
		{
			next->prio_d--;
			next = aux;
		}
		else if(aux->prio_d > -20)
            aux->prio_d--;
	}

	next->prio_d = next->prio_s;
	return next;
}

void task_suspend (task_t *task, task_t **queue)
{
    if (queue)
    {
        task_t *aux = task != NULL ? task : currentTask;
        queue_remove((queue_t**)&(aux->wqueue), (queue_t*)aux);
        queue_append((queue_t**)queue, (queue_t*)aux);
        aux->wqueue = *queue;
        aux->status = suspensa;
    }
}

void task_resume (task_t *task)
{
    if (task->wqueue != NULL)
    {
        queue_remove((queue_t**)&(task->wqueue), (queue_t*)task);
    }

    queue_append((queue_t**)&ready_queue, (queue_t*)task);
    task->wqueue = ready_queue;
    task->status = pronta;
}

void task_setprio (task_t *task, int prio)
{
    int pri;
    if (prio > 20)
        pri = 20;
    else if (prio < -20)
        pri = -20;
    else
        pri = prio;

    task_t *t = task != NULL ? task : currentTask;
    t->prio_s = pri;
    t->prio_d = pri;
}

int task_getprio (task_t *task)
{
    task_t *t = task != NULL ? task : currentTask;
    return t->prio_s;
}

void tick_handler(int signum)
{
    sys_time++;
    currentTask->proc_time++;

    if (currentTask != &dispatcherTask)
    {
        currentTask->ticks_t--;

        if (currentTask->ticks_t == 0)
        {
            task_yield();
        }
    }
}

void set_timer(int value_sec, int value_usec, int interval_sec, int interval_usec)
{
    action.sa_handler = tick_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction (SIGALRM, &action, 0) < 0)
    {
        perror("Erro em sigaction: ");
        exit(1);
    }

    timer.it_value.tv_sec  = value_sec;
    timer.it_value.tv_usec = value_usec;
    timer.it_interval.tv_sec = interval_sec;
    timer.it_interval.tv_usec = interval_usec;

    if(setitimer (ITIMER_REAL, &timer, 0) < 0)
    {
        perror ("Erro em setitimer: ");
        exit (1) ;
    }
}

unsigned int systime ()
{
    return sys_time;
}
