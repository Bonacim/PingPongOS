#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <bits/sigaction.h>
#include <sys/time.h>

#include "pingpong.h"
#include "queue.h"

#define _XOPEN_SOURCE 600
#define STACKSIZE 32768
#define TICKS 20
//#define DEBUG

task_t *ready_queue, *suspended_queue, *sleeping_queue;
task_t *current_task;
task_t main_task, dispatcher_task;

int newTaskID, userTasks;
unsigned int systemTime;

int allocate_stack (task_t *task);
void dispatcher_body ();
task_t* scheduler ();
void clear_task (task_t *task);
void set_timer (int sec, int usec);
void tick_handler ();
void wakeUp_tasks ();

void pingpong_init ()
{
    setvbuf (stdout, 0, _IONBF, 0);

    newTaskID = 0;
    userTasks = 0;
    systemTime = 0;

    ready_queue = NULL;
    suspended_queue = NULL;

    task_create (&main_task, NULL, "MAIN");
    current_task = &main_task;

    task_create (&dispatcher_task, dispatcher_body, "DISPATCHER");
    task_yield();

    set_timer (0, 1000);
}

int task_create (task_t *task, void (*start_func)(void *), void *arg)
{

    task->t_status = New;

    if (allocate_stack (task) == 0)
    {
        return -1;
    }

    if (*start_func)
    {
        makecontext (&task->t_context, (void*)(*start_func), 1, arg);
    }

    task->t_id = newTaskID++;

    if (task == &dispatcher_task)
    {
        task->next = NULL;
        task->prev = NULL;
        task->t_class = System;
        task->t_status = Ready;
    }

    else
    {
        if (task == &main_task)
        {
            task->current_queue = NULL;
            task->t_status = Running;
        }

        else
        {
            queue_append ((queue_t **)&ready_queue, (queue_t*)task);
            task->current_queue = &ready_queue;
            task->t_status = Ready;
        }

        userTasks++;
        task_setprio(task, 0);
        task->t_class = User;
    }

    task->proc_time = 0;
    task->activations = 0;
    task->created_at = systemTime;

    #ifdef DEBUG
    printf ("task_create: criou tarefa %d\n", task->t_id);
    #endif

    return task->t_id;
}

int task_switch (task_t *task)
{
    int status;
    task_t *prev_task;

    prev_task = current_task;
    current_task = task;

    #ifdef DEBUG
    printf ("task_switch: trocando contexto %d -> %d\n", prev_task->t_id, task->t_id);
    #endif

    current_task->activations++;
    status = swapcontext (&prev_task->t_context, &task->t_context);

    if (status == -1)
    {
        current_task = prev_task;
        current_task->activations--;
    }

    return status;
}

void task_exit (int exitCode)
{
    current_task->t_class = System;

    int queueSize;
    task_t *aux;

    #ifdef DEBUG
    printf ("task_exit: tarefa %d sendo encerrada. exit_code: %d\n", task_id(), exitCode);
    #endif

    //clear_task (current_task);

    aux = suspended_queue;

    for (queueSize = queue_size ((queue_t*)suspended_queue); queueSize > 0; queueSize--)
    {
        if (aux->join == current_task)
        {
            aux->join = NULL;
            aux->join_exitCode = exitCode;
            aux = aux->next;
            task_resume(aux->prev);
        }
        else
        {
            aux = aux->next;
        }
    }

    current_task->exec_time = systemTime - current_task->created_at;

    printf("Task %d exit: execution time %d ms, processor time %d ms, %d activations\n", current_task->t_id, current_task->exec_time, current_task->proc_time, current_task->activations);

    current_task->t_status = Terminated;

    if (current_task != &dispatcher_task)
    {
        userTasks--;
        current_task->t_class = User;
        task_switch (&dispatcher_task);
    }
}

int task_id ()
{
    return current_task->t_id;
}

void task_yield ()
{
    queue_append ((queue_t **)&ready_queue, (queue_t*)current_task);
    current_task->current_queue = &ready_queue;
    current_task->t_status = Ready;

    task_switch (&dispatcher_task);
}

void dispatcher_body ()
{

    task_t *next;
    while (userTasks > 0)
    {
        next = scheduler ();

        if (next)
        {
            queue_remove ((queue_t **)&ready_queue, (queue_t*)next);
            next->current_queue = NULL;
            next->t_ticks = TICKS;
            next->t_status = Running;
            task_switch (next);
        }

        wakeUp_tasks ();
    }

    task_exit (0);
}

task_t* scheduler ()
{
    int q_size;

    q_size = queue_size ((queue_t*)ready_queue);

    if (q_size)
    {
        task_t *aux, *next;

        aux = next = ready_queue;
        q_size--;

        while (q_size)
        {
            aux = aux->next;

            if ((aux->t_dprio < next->t_dprio) || (aux->t_dprio == next->t_dprio && aux->t_sprio < next->t_sprio))
            {
                next->t_dprio--;
                next = aux;
            }

            else
            {
                aux->t_dprio--;
            }

            q_size--;
        }

        next->t_dprio = next->t_sprio;
        return next;
    }

    else
    {
        return NULL;
    }
}

void task_setprio (task_t *task, int prio)
{
    task_t *aux = task == NULL ? current_task : task;

    aux->t_sprio = prio > 20 ? 20 : prio;
    aux->t_sprio = prio < -20 ? -20 : prio;
    aux->t_dprio = aux->t_sprio;
}

int task_getprio (task_t *task)
{
    return task == NULL ? current_task->t_sprio : task->t_sprio;
}

unsigned int systime ()
{
    return systemTime;
}

void task_suspend (task_t *task, task_t **queue)
{
    if (queue)
    {
        task_t *aux = task == NULL ? current_task : task;

        if (aux->current_queue)
        {
            queue_remove ((queue_t **)aux->current_queue, (queue_t*)aux);
            aux->current_queue = NULL;
        }

        queue_append ((queue_t **)queue, (queue_t*)aux);
        aux->current_queue = queue;
        aux->t_status = Suspended;
    }
}

void task_resume (task_t *task)
{
    if (task->current_queue)
    {
        queue_remove ((queue_t **)task->current_queue, (queue_t*)task);
        task->current_queue = NULL;
    }

    queue_append ((queue_t **)&ready_queue, (queue_t*)task);
    task->current_queue = &ready_queue;
    task->t_status = Ready;
}

int task_join (task_t *task)
{
    current_task->t_class = System;

    current_task->join_exitCode = -1;

    if (task)
    {
        if (task->t_status != Terminated)
        {
            current_task->join = task;
            task_suspend(current_task, &suspended_queue);
            task_switch(&dispatcher_task);
        }
    }

    current_task->t_class = User;

    return current_task->join_exitCode;
}

void task_sleep (int t)
{
    current_task->t_class = System;

    task_suspend (NULL, &sleeping_queue);

    current_task->wake_up_at = systemTime + (t * 1000);

    task_switch(&dispatcher_task);

    current_task->t_class = User;
}

void wakeUp_tasks ()
{
    int queueSize;
    task_t *aux;

    aux = sleeping_queue;

    for (queueSize = queue_size ((queue_t*)sleeping_queue); queueSize > 0; queueSize--)
    {
        if (systemTime >= aux->wake_up_at)
        {
            aux = aux->next;
            task_resume(aux->prev);
        }
        else
        {
            aux = aux->next;
        }
    }
}

int allocate_stack (task_t *task)
{
    char *stack;
    stack = malloc (STACKSIZE);

    getcontext (&task->t_context);

    if (stack)
    {
        task->t_context.uc_stack.ss_sp = stack;
        task->t_context.uc_stack.ss_size = STACKSIZE;
        task->t_context.uc_stack.ss_flags = 0;
        task->t_context.uc_link = 0;
        return 1;
    }
    else
    {
        perror ("Erro na criação da pilha: ");
        return 0;
    }
}

void clear_task (task_t *task)
{
    task->prev = NULL;
    task->next = NULL;
    task->t_context.uc_stack.ss_size = 0;
    //free(task->t_context.uc_stack.ss_sp);
}

void tick_handler ()
{
    systemTime++;
    current_task->proc_time++;

    if (current_task->t_class == User)
    {
        current_task->t_ticks--;

        if (current_task->t_ticks == 0)
        {
            task_yield();
        }
    }
}

void set_timer (int sec, int usec)
{
    static struct sigaction action;
    struct itimerval timer;

    action.sa_handler = tick_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction (SIGALRM, &action, 0) < 0)
    {
        perror("Erro em sigaction: ");
        exit(1);
    }

    timer.it_value.tv_sec  = sec;
    timer.it_value.tv_usec = usec;
    timer.it_interval.tv_sec = sec;
    timer.it_interval.tv_usec = usec;

    if(setitimer (ITIMER_REAL, &timer, 0) < 0)
    {
        perror ("Erro em setitimer: ");
        exit (1) ;
    }
}
