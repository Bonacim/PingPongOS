// Alexandre Nadolni Bonacim
// João Felipe Sarggin Machado

#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <bits/sigaction.h>
#include <sys/time.h>

#include "pingpong.h"
#include "queue.h"

//#define DEBUG
#define STACKSIZE 32768
#define _XOPEN_SOURCE 600
#define TICKS 20

// datatypes utilizados //
task_t mainTask, dispatcherTask, *currentTask, *ready_queue, *suspend_queue;

struct sigaction action;
struct itimerval timer;

// variáveis globais //

int newTaskID, userTasks;
unsigned int sys_time;

// funções internas //

void dispatcher_body();
task_t *scheduler();
void tick_handler(int signum);
void set_timer(int value_sec, int value_usec, int interval_sec, int interval_usec);

// INIT: Inicializa os datatypes, variáveis globais, a tarefa MAIN e a tarefa DISPATCHER //

void pingpong_init ()
{
    sys_time = 0;
    setvbuf (stdout, 0, _IONBF, 0);

    newTaskID = 0;
    userTasks = 0;

    mainTask.tid = newTaskID;                                   // Cria a tarefa main e a define como tarefa
    mainTask.next = &mainTask;                                  // em execução
    mainTask.prev = &mainTask;
    currentTask = &mainTask;

    task_create(&dispatcherTask, dispatcher_body, "Dispatcher");    // Cria o DISPATCHER

    ready_queue = NULL;
    suspend_queue = NULL;

    set_timer(0, 1000, 0, 1000);                                    // Inicializa o relógio de interrupções

    getcontext(&(mainTask.context));
}

// TASK CREATE: Cria uma tarefa, inicializando-a e dando a seu devido contexto //

int task_create (task_t *task, void (*start_func)(void *), void *arg)
{
    char *stack ;
    stack = malloc (STACKSIZE) ;

    if (stack)                                          // Se o MALLOC for bem sucedido, aloca a STACK da tarefa
    {
        task->next = NULL;
        task->prev = NULL;
        task->wqueue = NULL;

        task->tid = ++newTaskID;
        task->created_at = sys_time;
		task->proc_time = 0;
		task->activations = 0;

        getcontext(&(task->context));
        task->context.uc_stack.ss_sp = stack;
        task->context.uc_stack.ss_size = STACKSIZE;
        task->context.uc_stack.ss_flags = 0;
        task->context.uc_link = 0;

        makecontext (&(task->context), (void *)(*start_func), 1, arg);      // Define a função de contexto da tarefa

        #ifdef DEBUG
        printf ("task_create: criou tarefa %d\n", task->tid);
        #endif

        if (task != &dispatcherTask)                                        // Outras tarefas vão para a fila de
        {                                                                   // prontas
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

// TASK SWITCH: Realiza a troca de contexto entre as tarefas, contabilizando as ativações e //
//              definindo a task atual em execução //

int task_switch (task_t *task)
{
    task_t *aux = currentTask;                              // Guarda o ponteiro da tarefa que vai sair, caso a
    currentTask = task;                                     // troca de contexto seja mal sucedida
    currentTask->activations++;

    #ifdef DEBUG
    printf ("task_switch: trocando contexto %d -> %d\n", aux->tid, task->tid);
    #endif

    if (swapcontext (&(aux->context), &(task->context)) < 0)    // Se a troca falhar, retorna para a tarefa anterior
    {
        currentTask = aux;
        return -1;
    }

    return 0;
}

// TASK EXIT: Encera uma tarefa, devolvendo o controle ao DISPATCHER e contabilizando seus tempos //
//            de execução e processamento. //
//            O DISPATCHER não passa o controle para ninguém //

void task_exit (int exitCode)
{
    #ifdef DEBUG
    printf ("task_exit: tarefa %d sendo encerrada\n", currentTask->tid);        // Contabiliza tempos
    #endif
    int exec_time = sys_time - currentTask->created_at;
    printf("Task %d exit: execution time %d ms, processor time %d ms, %d activations\n",currentTask->tid,exec_time,currentTask->proc_time,currentTask->activations);

    if (currentTask == &dispatcherTask)                             // o DISPATCHER devolve execução para MAIN
    {
        task_switch(&mainTask);
    }

    queue_remove((queue_t**)&ready_queue, (queue_t*)currentTask);   // Remove a tarefa da fila de prontas
    userTasks--;

    task_switch(&dispatcherTask);
}

// TASK ID: Retorna o id da task em execução //

int task_id ()
{
    return currentTask->tid;
}

// TASK YIELD: Retorna a tarefa em execução para a fila de prontas e devolve o controle ao DISPATCHER //

void task_yield ()
{
    task_switch(&dispatcherTask);
}

// DISPATCHER: Aciona o SCHEDULER, define o quantum da tarefa que executará e passa o controle para a mesma //
//             O DISPATCHER também acorda tarefas suspensas pela chamada TASK SLEEP //

void dispatcher_body ()
{
    task_t *next;

    while (userTasks > 0)
    {
        next = scheduler();             // Escolhe a próxima tarefa a ganhar o processador

        if (next)
        {
            next->ticks_t = TICKS;      // Realiza a devida torca entre as tarefas
            task_switch (next);
        }
    }
    task_exit(0);
}

// SCHEDULER: Escalonador por PRIORIDADES DINÂMICAS com AGING -1 (Usa uma escala negativa) //

task_t *scheduler()
{
	task_t* aux = ready_queue;
    task_t* next = aux;

	while(aux != NULL && aux->next != ready_queue)      // Varre a fila de tarefas prontas, envelhecendo as que
	{                                                   // não foram escolhidas, e reiniciando a prioridade
		aux = aux->next;                                // dinâmica da escolhida

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

// TASK SUSPEND: Suspende a tarefa informada, ou a atual em execução, e a coloca na fila informada //
//               Se a fila não for informada, não faz nada //

void task_suspend (task_t *task, task_t **queue)
{
    if (queue)
    {
        task_t *aux = task != NULL ? task : currentTask;                // Tira da fila atual para colocar na
        queue_remove((queue_t**)&(aux->wqueue), (queue_t*)aux);         // fila de suspensa
        queue_append((queue_t**)queue, (queue_t*)aux);
        aux->wqueue = *queue;
        aux->status = suspensa;
    }
}

// TASK RESUME: Tira uma tarefa informada da fila atual e a coloca na fila de prontas //

void task_resume (task_t *task)
{
    if (task->wqueue != NULL)                                       // Se estiver em alguma fila, retira
    {
        queue_remove((queue_t**)&(task->wqueue), (queue_t*)task);
    }

    queue_append((queue_t**)&ready_queue, (queue_t*)task);          // E coloca na fila de prontas
    task->wqueue = ready_queue;
    task->status = pronta;
}

// TASK SETPRIO: Define a prioridade estática de uma tarefa //

void task_setprio (task_t *task, int prio)
{
    int pri;                                    // Verifica se o valor é válido na escala
    if (prio > 20)
        pri = 20;
    else if (prio < -20)
        pri = -20;
    else
        pri = prio;

    task_t *t = task != NULL ? task : currentTask;      // Define a prioridade estática
    t->prio_s = pri;
    t->prio_d = pri;
}

// TASK GETPRIO: Retorna a prioridade estática da tarefa informada ou da atual em execução //

int task_getprio (task_t *task)
{
    task_t *t = task != NULL ? task : currentTask;
    return t->prio_s;
}

// TICK HANDLER: Tratador de interrupções, responsável pela preempção das tarefas em modo USER //

void tick_handler(int signum)
{
    sys_time++;
    if (currentTask != &mainTask && currentTask != &dispatcherTask)     // MAIN E DISPATCHER não podem ser
    {                                                                   // preemptadas
        currentTask->ticks_t--;
        currentTask->proc_time++;
        if (currentTask->ticks_t == 0)
        {
            task_yield();
        }
    }
}

// SET TIMER: Inicializa um relógio de interrupções periódicas (1 ms) //

void set_timer(int value_sec, int value_usec, int interval_sec, int interval_usec)
{
    action.sa_handler = tick_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction (SIGALRM, &action, 0) < 0)            // Tenta definir as interrupções junto ao Sistema Operacional
    {
        perror("Erro em sigaction: ");
        exit(1);
    }

    timer.it_value.tv_sec  = value_sec;                 // Define o tempo da primeira e das seguintes interrupções
    timer.it_value.tv_usec = value_usec;
    timer.it_interval.tv_sec = interval_sec;
    timer.it_interval.tv_usec = interval_usec;

    if(setitimer (ITIMER_REAL, &timer, 0) < 0)          // Tenta definir o timer junto ao Sistema Operacional
    {
        perror ("Erro em setitimer: ");
        exit (1) ;
    }
}

// SYSTIME: Tempo decorrido desde a inicialização (Medido em Ticks de 1 ms) //

unsigned int systime ()
{
    return sys_time;
}
