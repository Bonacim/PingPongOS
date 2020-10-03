#include <stdio.h>
#include <stdlib.h>
#include "pingpong.h"
#include "datatypes.h"
#include "queue.h"

//#define DEBUG
#define STACKSIZE 32768
#define _XOPEN_SOURCE 600

task_t mainTask, dispatcherTask, *currentTask, *ready_queue, *suspend_queue;
int newTaskID, userTasks;

void dispatcher_body (void *arg);
task_t *scheduler();

void pingpong_init ()
{
    setvbuf (stdout, 0, _IONBF, 0);

    newTaskID = 0;
    userTasks = 0;

    mainTask.tid = newTaskID;
    mainTask.next = &mainTask;
    mainTask.prev = &mainTask;
    currentTask = &mainTask;

    task_create(&dispatcherTask, dispatcher_body, "Dispatcher");

    ready_queue = NULL;
    suspend_queue = NULL;

    getcontext(&(mainTask.context));
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

        task->tid = ++newTaskID;

        getcontext(&(task->context));
        task->context.uc_stack.ss_sp = stack;
        task->context.uc_stack.ss_size = STACKSIZE;
        task->context.uc_stack.ss_flags = 0;
        task->context.uc_link = 0;

        makecontext (&(task->context), (void *)(*start_func), 1, arg);

        #ifdef DEBUG
        printf ("task_create: criou tarefa %d\n", task->tid);
        #endif

        if (task != &dispatcherTask)
        {
            queue_append((queue_t**)&ready_queue, (queue_t*)task);
            task->wqueue = ready_queue;
            task->status = pronta;
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

    if (currentTask == &dispatcherTask)
    {
        task_switch(&mainTask);
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

void dispatcher_body (void *arg)
{
    task_t *next;

    while (userTasks > 0)
    {
        next = scheduler();

        if (next)
        {
            queue_remove((queue_t**)&ready_queue, (queue_t*)next);
            queue_append((queue_t**)&ready_queue, (queue_t*)next);
            task_switch (next);
        }
    }
    task_exit(0);
}

task_t *scheduler()
{
    return ready_queue;
}

void task_suspend (task_t *task, task_t **queue)
{
    if (queue)
    {
        task_t *aux = task;

        if (aux == NULL)
        {
            aux = currentTask;
        }

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
