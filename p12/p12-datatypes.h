// Alexandre Nadolni Bonacim
// João Felipe Sarggin Machado
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
    int barriExit;
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
typedef struct barrier_t
{
    struct barrier_t *prev, *next;
    int n_tasks;
    struct task_t *task_queue;
} barrier_t ;

// estrutura que define uma fila de mensagens
typedef struct
{
  struct mqueue_t *prev, *next;
	int max, size;
	char *msgs;
    int first_msg, last_msg;
	semaphore_t sem_buffer, sem_item, sem_vaga ;
} mqueue_t ;

#endif
