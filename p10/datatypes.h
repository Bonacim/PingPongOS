// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DAINF UTFPR
// Versão 1.0 -- Março de 2015
//
// Estruturas de dados internas do sistema operacional

#ifndef __DATATYPES__
#define __DATATYPES__

#include <ucontext.h>

typedef enum {System, User} task_class;
typedef enum {New, Ready, Running, Suspended, Terminated} task_status;

// Estrutura que define uma tarefa
typedef struct task_t
{
    struct task_t *prev, *next;
    int t_id;
    ucontext_t t_context;
    int t_sprio;
    int t_dprio;
    unsigned int t_ticks;
    task_class t_class;
    task_status t_status;
    unsigned int created_at;
    unsigned int exec_time;
    unsigned int proc_time;
    unsigned int activations;
    struct task_t *join;
    struct task_t **current_queue;
    int join_exitCode;
    unsigned int wake_up_at;
    int semapExit;
} task_t ;

// estrutura que define um semáforo
typedef struct
{
  struct task_t *prev, *next;
  int value;
  struct queue_t *queue;
} semaphore_t ;

// estrutura que define um mutex
typedef struct
{
  // preencher quando necessário
} mutex_t ;

// estrutura que define uma barreira
typedef struct
{
  // preencher quando necessário
} barrier_t ;

// estrutura que define uma fila de mensagens
typedef struct
{
  // preencher quando necessário
} mqueue_t ;

#endif
