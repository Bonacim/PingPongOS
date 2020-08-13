#include "pingpong.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>

// funções gerais ==============================================================
#define STACKSIZE 32768		/* tamanho de pilha das threads */
task_t taskMain, *taskAtual;
int newTaskId;

// Inicializa o sistema operacional; deve ser chamada no inicio do main()
void pingpong_init ()
{
    /* desativa o buffer da saida padrao (stdout), usado pela função printf */
    setvbuf (stdout, 0, _IONBF, 0) ;
    taskMain.tid = 0;
    taskMain.next = &taskMain;
    taskMain.prev = &taskMain;
    taskAtual = &taskMain;
    newTaskId = 1;
}

// gerência de tarefas =========================================================

// Cria uma nova tarefa. Retorna um ID> 0 ou erro.
int task_create (task_t *task,			// descritor da nova tarefa
                 void (*start_func)(void *),	// funcao corpo da tarefa
                 void *arg)// argumentos para a tarefa
{
    getcontext(&(task->context));

    char *stack = malloc (STACKSIZE) ;
    if (stack)
    {
        task->context.uc_stack.ss_sp = stack ;
        task->context.uc_stack.ss_size = STACKSIZE;
        task->context.uc_stack.ss_flags = 0;
        task->context.uc_link = 0;

        makecontext(&(task->context), (void*)(*start_func), 1, arg);

        task->tid = newTaskId;
        newTaskId++;

        queue_append((queue_t**)&taskMain,(queue_t*)task);

        #ifdef DEBUG
        printf ("task_create: criou tarefa %d\n", task->tid) ;
        #endif
        
        return task->tid;
    }
    else
    {
        perror ("Erro na cria��o da pilha: ");
        return -1;
    }
}			

// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit (int exitCode)
{    
    task_t *aux = taskAtual;
    #ifdef DEBUG
    printf ("task_exit: tarefa %d sendo encerrada\n", aux->tid) ;
    #endif
    queue_remove((queue_t**)&taskMain,(queue_t*)aux);
    task_switch(&taskMain);
}

// alterna a execução para a tarefa indicada
int task_switch (task_t *task)
{
    task_t *aux = taskAtual;
    taskAtual = task;   

    #ifdef DEBUG
    printf ("task_switch: trocando contexto %d -> %d\n",aux->tid, task->tid) ;
    #endif
    if (swapcontext(&(aux->context),&(task->context)) == -1)
    {
        taskAtual = aux;
        return -1;
    }
    return 0;
}

// retorna o identificador da tarefa corrente (main eh 0)
int task_id ()
{
    return taskAtual->tid;
}