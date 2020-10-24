// Alexandre Nadolni Bonacim
// João Felipe Sarggin Machado

#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <bits/sigaction.h>
#include <sys/time.h>
#include <string.h>

#include "pingpong.h"
#include "queue.h"

#define _XOPEN_SOURCE 600
#define STACKSIZE 32768
#define TICKS 20
//#define DEBUG

// datatypes utilizados //

task_t *ready_queue, *suspended_queue, *sleeping_queue;
queue_t* semaphore_queue;
queue_t* mqueue;
barrier_t *barrier_queue;
task_t *current_task;
task_t main_task, dispatcher_task;

// variáveis globais //

int newTaskID, userTasks;
unsigned int systemTime;

// funções internas //

int allocate_stack (task_t *task);
void dispatcher_body ();
task_t* scheduler ();
void clear_task (task_t *task);
void set_timer (int sec, int usec);
void tick_handler ();
void wakeUp_tasks ();

// INIT: Inicializa os datatypes, variáveis globais, a tarefa MAIN e a tarefa DISPATCHER //

void pingpong_init ()
{
    setvbuf (stdout, 0, _IONBF, 0);

    newTaskID = 0;
    userTasks = 0;
    systemTime = 0;

    ready_queue = NULL;
    suspended_queue = NULL;

    semaphore_queue = NULL;
    barrier_queue = NULL;
    mqueue = NULL;

    task_create (&main_task, NULL, "MAIN");                                 // Cria a tarefa main e define ela como
    current_task = &main_task;                                              // a tarefa atual em execução

    task_create (&dispatcher_task, dispatcher_body, "DISPATCHER");          // Cria a tarefa dispatcher e já a
    task_yield();                                                           // inicializa

    set_timer (0, 1000);                                                    // Ticks do sistema
}

// TASK CREATE: Cria uma tarefa, inicializando-a e dando a seu devido contexto //

int task_create (task_t *task, void (*start_func)(void *), void *arg)
{

    task->t_status = New;                                                   // Define status para Nova tarefa

    if (allocate_stack (task) == 0)                                         // Tenta alocar a STACK da tarefa
    {
        return -1;
    }

    if (*start_func)                                                        // Define a função de contexto da tarefa
    {
        makecontext (&task->t_context, (void*)(*start_func), 1, arg);
    }

    task->t_id = newTaskID++;                                               // Define o ID da nova tarefa

    if (task == &dispatcher_task)                                           // Inicializa o DISPATCHER
    {
        task->next = NULL;
        task->prev = NULL;
        task->t_class = System;                                             // Tarefa de Sistema
        task->t_status = Ready;
    }

    else
    {
        if (task == &main_task)                                             // Inicializa a tarefa main
        {
            task->current_queue = NULL;
            task->t_status = Running;                                       // Começa executando
        }

        else                                                                // Demais tarefas vão para fila de pronta
        {
            queue_append ((queue_t **)&ready_queue, (queue_t*)task);
            task->current_queue = &ready_queue;
            task->t_status = Ready;
        }

        userTasks++;                                                        // Controle de execução do DISPATCHER
        task_setprio(task, 0);                                              // Prioridade padrão é 0
        task->t_class = User;                                               // Em modo USER
    }

    task->proc_time = 0;                                                    // Inicializa as variáveis de
    task->activations = 0;                                                  // contabilização
    task->created_at = systemTime;

    #ifdef DEBUG
    printf ("task_create: criou tarefa %d\n", task->t_id);
    #endif

    return task->t_id;
}

// TASK SWITCH: Realiza a troca de contexto entre as tarefas, contabilizando as ativações e //
//              definindo a task atual em execução //

int task_switch (task_t *task)
{
    int status;
    task_t *prev_task;
                                                                            // É armazenado uma cópia da referência
    prev_task = current_task;                                               // da tarefa que está sendo trocada
    current_task = task;                                                    // para caso uma falha ocorra, ela voltar
                                                                            // a executar
    #ifdef DEBUG
    printf ("task_switch: trocando contexto %d -> %d\n", prev_task->t_id, task->t_id);
    #endif

    current_task->activations++;                                            // Contabiliza mais uma ativação
    status = swapcontext (&prev_task->t_context, &task->t_context);         // Realiza a troca de contexto

    if (status == -1)                                                       // Reverte caso haja falha na troca
    {
        current_task = prev_task;
        current_task->activations--;
    }

    return status;
}

// TASK EXIT: Encera uma tarefa, devolvendo o controle ao DISPATCHER e contabilizando seus tempos //
//            de execução e processamento. //
//            O DISPATCHER não passa o controle para ninguém //

void task_exit (int exitCode)
{
    current_task->t_class = System;                                          // Desativa preempção

    int queueSize;
    task_t *aux;

    #ifdef DEBUG
    printf ("task_exit: tarefa %d sendo encerrada. exit_code: %d\n", task_id(), exitCode);
    #endif

    //clear_task (current_task);

    aux = suspended_queue;

    for (queueSize = queue_size ((queue_t*)suspended_queue); queueSize > 0; queueSize--)
    {
        if (aux->join == current_task)                                         // Acorda tarefas que fizeram JOIN
        {                                                                      // a tarefa que está tentando sai
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

    current_task->t_status = Terminated;                                      // Encerra a tarefa, mostrando informações
                                                                              // de execução e passando o controle
    if (current_task != &dispatcher_task)                                     // para o DISPATCHER
    {
        userTasks--;
        current_task->t_class = User;
        task_switch (&dispatcher_task);
    }
}

// TASK ID: Retorna o id da task em execução //

int task_id ()
{
    return current_task->t_id;
}

// TASK YIELD: Retorna a tarefa em execução para a fila de prontas e devolve o controle ao DISPATCHER //

void task_yield ()
{
    queue_append ((queue_t **)&ready_queue, (queue_t*)current_task);
    current_task->current_queue = &ready_queue;
    current_task->t_status = Ready;

    task_switch (&dispatcher_task);
}

// DISPATCHER: Aciona o SCHEDULER, define o quantum da tarefa que executará e passa o controle para a mesma //
//             O DISPATCHER também acorda tarefas suspensas pela chamada TASK SLEEP //

void dispatcher_body ()
{

    task_t *next;
    while (userTasks > 0)
    {
        next = scheduler ();                                            // Escolhe a próxima tarefa a executar

        if (next)
        {
            queue_remove ((queue_t **)&ready_queue, (queue_t*)next);    // Prepara e realiza a troca entre as
            next->current_queue = NULL;                                 // tarefas
            next->t_ticks = TICKS;
            next->t_status = Running;
            task_switch (next);
        }

        wakeUp_tasks ();                                                // Verifica se pode acordar alguma tarefa
    }                                                                   // dormindo

    task_exit (0);
}

// SCHEDULER: Escalonador por PRIORIDADES DINÂMICAS com AGING -1 (Usa uma escala negativa) //

task_t* scheduler ()
{
    int q_size;

    q_size = queue_size ((queue_t*)ready_queue);

    if (q_size)                                             // Varre a fila de tarefas prontas atualizando a prioridade
    {                                                       // dinâmica das que não foram escolhidas, e ajustando
        task_t *aux, *next;                                 // a prioridade dinâmica da que foi escolhida de volta
                                                            // para a prioridade estática
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

// TASK SETPRIO: Define a prioridade estática de uma tarefa //

void task_setprio (task_t *task, int prio)
{
    task_t *aux = task == NULL ? current_task : task;

    aux->t_sprio = prio > 20 ? 20 : prio;
    aux->t_sprio = prio < -20 ? -20 : prio;
    aux->t_dprio = aux->t_sprio;
}

// TASK GETPRIO: Retorna a prioridade estática da tarefa informada ou da atual em execução //

int task_getprio (task_t *task)
{
    return task == NULL ? current_task->t_sprio : task->t_sprio;
}

// SYSTIME: Tempo decorrido desde a inicialização (Medido em Ticks de 1 ms) //

unsigned int systime ()
{
    return systemTime;
}

// TASK SUSPEND: Suspende a tarefa informada, ou a atual em execução, e a coloca na fila informada //
//               Se a fila não for informada, não faz nada //

void task_suspend (task_t *task, task_t **queue)
{
    if (queue)
    {
        task_t *aux = task == NULL ? current_task : task;                   // Tarefa atual ou uma específica

        if (aux->current_queue)                                             // Tira da fila atual da tarefa
        {
            queue_remove ((queue_t **)aux->current_queue, (queue_t*)aux);
            aux->current_queue = NULL;
        }

        queue_append ((queue_t **)queue, (queue_t*)aux);                    // Insere ela na fila de suspensas
        aux->current_queue = queue;
        aux->t_status = Suspended;
    }
}

// TASK RESUME: Tira uma tarefa informada da fila atual e a coloca na fila de prontas //

void task_resume (task_t *task)
{
    if (task->current_queue)                                                // Se esta em alguma fila, então remove
    {
        queue_remove ((queue_t **)task->current_queue, (queue_t*)task);
        task->current_queue = NULL;
    }

    queue_append ((queue_t **)&ready_queue, (queue_t*)task);                // E coloca de volta na fila de prontas
    task->current_queue = &ready_queue;
    task->t_status = Ready;
}

// TASK JOIN: Suspende a tarefa em execução, e só retorna quando a tarefa informada encerrar sua execução //

int task_join (task_t *task)
{
    current_task->t_class = System;                             // Desabilita preempção, código critico!

    current_task->join_exitCode = -1;

    if (task)
    {
        if (task->t_status != Terminated)                       // Verifica se o JOIN é valido e o realiza
        {
            current_task->join = task;
            task_suspend(current_task, &suspended_queue);
            task_switch(&dispatcher_task);
        }
    }

    current_task->t_class = User;

    return current_task->join_exitCode;
}

// TASK SLEEP: Suspende uma tarefa por t segundos (Vai para a fila de tarefas adormecidas) //

void task_sleep (int t)
{
    current_task->t_class = System;

    task_suspend (NULL, &sleeping_queue);                   // Suspende a tarefa atual

    current_task->wake_up_at = systemTime + (t * 1000);     // Calcula quando ela deve acordar (relativo ao systime)

    task_switch(&dispatcher_task);

    current_task->t_class = User;
}

// WAKEUP TASKS: Função interna que auxilia o DISPATCHER a acordas as tarefas que estão dormindo no momento //
//               correto //

void wakeUp_tasks ()
{
    int queueSize;
    task_t *aux;

    aux = sleeping_queue;

    for (queueSize = queue_size ((queue_t*)sleeping_queue); queueSize > 0; queueSize--)
    {
        if (systemTime >= aux->wake_up_at)                              // Percorre a fila de tarefas dormentes
        {                                                               // para verificar se o tempo de acordar
            aux = aux->next;                                            // já chegou
            task_resume(aux->prev);
        }
        else
        {
            aux = aux->next;
        }
    }
}

// ALLOCATE STACK: Função interna auxiliar que aloca a STACK da tarefa para a função TASK CREATE //

int allocate_stack (task_t *task)
{
    char *stack;
    stack = malloc (STACKSIZE);

    getcontext (&task->t_context);

    if (stack)                                              // Se o MALLOC foi bem sucedido, então aloca a área
    {                                                       // de STACK da tarefa
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

// CLEAR TASK: Função auxiliar da TASK EXIT que desaloca a STACK da tarefa //

void clear_task (task_t *task)
{
    task->prev = NULL;
    task->next = NULL;
    task->t_context.uc_stack.ss_size = 0;
    //free(task->t_context.uc_stack.ss_sp);
}

// TICK HANDLER: Tratador de interrupções, responsável pela preempção das tarefas em modo USER //

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

// SET TIMER: Inicializa um relógio de interrupções periódicas (1 ms) //

void set_timer (int sec, int usec)
{
    static struct sigaction action;
    struct itimerval timer;

    action.sa_handler = tick_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction (SIGALRM, &action, 0) < 0)                    // Tenta registrar o evento junto ao Sistema Operacional
    {
        perror("Erro em sigaction: ");
        exit(1);
    }

    timer.it_value.tv_sec  = sec;                               // Define o tempo para a primeira interrupção, e
    timer.it_value.tv_usec = usec;                              // para as posteriores
    timer.it_interval.tv_sec = sec;
    timer.it_interval.tv_usec = usec;

    if(setitimer (ITIMER_REAL, &timer, 0) < 0)                  // Tenta registrar o timer junto ao Sistema Operacional
    {
        perror ("Erro em setitimer: ");
        exit (1) ;
    }
}

// SEM CREATE: Cria um semáforo com o valor informado //

int sem_create (semaphore_t *s, int value)
{
	if(s == NULL)                                                   // Verifica se o ponteiro é valido
		return -1;

	task_class save = current_task->t_class;                        // Desativa preempção
    current_task->t_class = System;

    queue_t* aux = (queue_t*)semaphore_queue;
    int found = 0;
    int i;
    for(i = queue_size((queue_t*)semaphore_queue); i > 0; i--)
    {
        if (aux == (queue_t*)s)
        {
            found = 1;                                              // Só cria o semáforo se ele ainda não consta
            break;                                                  // na fila de semáforos interna
        }
        aux = aux->next;
    }
	if(found)
	{
        current_task->t_class = save;
		return -1;
	}

	s->next = NULL;
	s->prev = NULL;
	queue_append(&semaphore_queue,(queue_t*)s);                     // Registra o semáforo na fila de semáforos

	s->value = value;
    current_task->t_class = save;
	return 0;
}

// SEM DOWN: Função bloqueante caso o numero máximo de tarefas naquele trecho de código tenha sido atingido //

int sem_down (semaphore_t *s)
{
	if(s == NULL)                                               // Verifica ponteiro
		return -1;

	task_class save = current_task->t_class;                    // Desativa preempção
    current_task->t_class = System;

    queue_t* aux = (queue_t*)semaphore_queue;
    int found = 0;
    int i;
    for(i = queue_size((queue_t*)semaphore_queue); i > 0; i--)
    {
        if (aux == (queue_t*)s)
        {
            found = 1;                                          // Procura ele na fila de semáforos, se não estiver
            break;                                              // ele ainda não foi inicializado
        }
        aux = aux->next;
    }
	if(!found)
	{
        current_task->t_class = save;
		return -1;
	}

	current_task->semapExit = 0;

	s->value--;
	if(s->value < 0)                                    // A tarefa é bloqueada caso o contador fique negativo
	{
		//queue_remove((queue_t**)&ready_queue, (queue_t*)current_task);
		queue_append(&s->queue,(queue_t*)current_task);

		current_task->t_status = Suspended;
		task_switch(&dispatcher_task);
	}

	current_task->t_class = save;
	return current_task->semapExit;
}

// SEM UP: Libera uma vaga naquele trecho de código, acordando a tarefa a mais tempo suspensa, esperando por acesso //

int sem_up (semaphore_t *s)
{
	task_t *aux;

	if(s == NULL)                                                           // Ponteiro Válido?
		return -1;

	task_class save = current_task->t_class;                                // Desabilita Preempção
    current_task->t_class = System;


    queue_t* sem_aux = (queue_t*)semaphore_queue;
    int found = 0;
    int i;
    for(i = queue_size((queue_t*)semaphore_queue); i > 0; i--)
    {
        if (sem_aux == (queue_t*)s)
        {
            found = 1;
            break;
        }
        sem_aux = sem_aux->next;
    }
	if(!found)                                                          // Procura o semáforo na fila, se não estiver
	{                                                                   // ele não foi inicializado
        current_task->t_class = save;
		return -1;
	}

	current_task->semapExit = 0;

	s->value++;

	if(s->value <= 0)                                                   // Se o contador está negativo ou nulo,
	{                                                                   // Há tarefa(s) a ser(em) acordada(s)
		aux = (task_t*)queue_remove(&s->queue,s->queue);
		aux->t_status = Ready;

		queue_append((queue_t**)&ready_queue,(queue_t*)aux);
	}

	current_task->t_class = save;
	return current_task->semapExit;
}

// SEM DESTROY: Destrói um semáforo, acordando todas as tarefas que esperavam pelo acesso a tal trecho //

int sem_destroy (semaphore_t *s)
{
	task_t *aux;

	if(s == NULL)                                               // Verifica ponteiro
		return -1;

	task_class save = current_task->t_class;                    // Desabilita preempção
    current_task->t_class = System;

    queue_t* sem_aux = (queue_t*)semaphore_queue;
    int found = 0;
    int i;
    for(i = queue_size((queue_t*)semaphore_queue); i > 0; i--)
    {
        if (sem_aux == (queue_t*)s)
        {
            found = 1;                                          // Procura o semáforo a destruir, se ele não está na
            break;                                              // fila, a chamada retorna erro.
        }
        sem_aux = sem_aux->next;
    }
	if(!found)
	{
        current_task->t_class = save;
		return -1;
	}


	while(s->queue != NULL)                                         // Desaloca o semáforo e acorda todas as tarefas
	{                                                               // que estavam bloqueadas nele
		aux = (task_t*)queue_remove (&s->queue,s->queue);
		aux->semapExit = -1;
		aux->t_status = Ready;

		queue_append((queue_t**)&ready_queue,(queue_t*)aux);
	}

	queue_remove (&semaphore_queue,(queue_t*)s);                    // Tira o semáforo da fila

	current_task->t_class = save;
	return 0;

}

// BARRIER CREATE: Cria uma barreira para até N tarefas //

int barrier_create (barrier_t *b, int N)
{
    current_task->t_class = System;                             // Impede preempção

    if (b == NULL || N < 1)                                     // Barreira tem que existir e N tem que ser maior
    {                                                           // que 1
        current_task->t_class = User;
        return -1;
    }

    else
    {
        int q_size;
        barrier_t *aux;

        aux = barrier_queue;

        for (q_size = queue_size ((queue_t*)barrier_queue); q_size > 0; q_size--)
        {
            if (aux == b)
            {
                current_task->t_class = User;                       // Verifica se a barreira já está registrada na fila,
                return -1;                                          // Se não estiver, faz o registro
            }

            aux = aux->next;
        }

        b->next = NULL;
        b->prev = NULL;
        queue_append ((queue_t**)&barrier_queue, (queue_t*)b);

        b->n_tasks = N;
        b->task_queue = NULL;

        current_task->t_class = User;
        return 0;
    }
}

// BARRIER JOIN: Tarefa sinaliza que chegou na barreira, se a N-ésima tarefa chegar nela, todas são acordadas //
//               e permitidas de prosseguir //

int barrier_join (barrier_t *b)
{
    current_task->t_class = System;                                 // Suspende preempções

    if (b == NULL)                                                  // Verifica ponteiro
    {
        current_task->t_class = User;
        return -1;
    }

    else
    {
        int found = 0;

        int q_size;
        barrier_t *aux;

        aux = barrier_queue;

        for (q_size = queue_size ((queue_t*)barrier_queue); q_size > 0; q_size--)
        {
            if (aux == b)
            {
                found = 1;                                  // Procura a barreira na fila, se não estiver, ela não
            }                                               // foi inicializada

            aux = aux->next;
        }

        if (found == 0)
        {
            current_task->t_class = User;
            return -1;
        }

        else
        {
            int tasks_quant;

            tasks_quant = queue_size ((queue_t*)b->task_queue);

            if (b->n_tasks - tasks_quant > 1)                       // Suspende as N - 1 primeiras tarefas que chegam
            {                                                       // a barreira
                task_suspend (current_task, &b->task_queue);

                current_task->barriExit = 0;

                current_task->t_class = User;

                task_switch (&dispatcher_task);
            }

            else                                                // A N-ésima tarefa a entrar, acorda todas as que estavam
            {                                                   // esperando na barreira
                task_t *aux;

                aux = b->task_queue;

                for (; tasks_quant > 0; tasks_quant--)
                {
                    aux = aux->next;
                    task_resume(aux->prev);
                }

                current_task->barriExit = 0;

                current_task->t_class = User;
            }

            return current_task->barriExit;
        }
    }
}

// BARRIER DESTROY: Destrói uma barreira e libera as tarefas que já aviam sinalizado sua chegada a mesma //

int barrier_destroy (barrier_t *b)
{
    current_task->t_class = System;                                 // Desativa preempção

    if (b == NULL)                                                  // Verifica Ponteiro
    {
        current_task->t_class = User;
        return -1;
    }

    else
    {
        int q_size;                                             // Procura pela barreira, e se achar, acorda as tarefas
        barrier_t *b_aux;                                       // espernado nela

        b_aux = barrier_queue;

        for (q_size = queue_size ((queue_t*)barrier_queue); q_size > 0; q_size--)
        {
            if (b_aux == b)
            {
                int tasks_quant;
                task_t *t_aux;
                t_aux = b->task_queue;

                for (tasks_quant = queue_size ((queue_t*)b->task_queue); tasks_quant > 0; tasks_quant--)
                {
                    t_aux->barriExit = -1;                                  // Código de erro pela barreira ter sido
                    t_aux = t_aux->next;                                    // destruida
                    task_resume (t_aux->prev);                              // Acorda as tarefas
                }

                queue_remove ((queue_t**)&barrier_queue, (queue_t*)b);      // Tira a barreira da fila de barreiras

                current_task->t_class = User;
                return 0;
            }

            b_aux = b_aux->next;
        }

        current_task->t_class = User;
        return -1;
    }
}

// MQUEUE CREATE: Cria uma fila no esquema produtor / consumidor //

int mqueue_create (mqueue_t *queue, int max, int size)
{
	if(queue == NULL)                                       // Verifica ponteiro
		return -1;

    task_class save = current_task->t_class;                // Desativa preempção
	current_task->t_class = System;

    queue_t* aux = (queue_t*)mqueue;
    int found = 0;
    int i;
    for(i = queue_size((queue_t*)mqueue); i > 0; i--)       // Procura pela message queue na fila
    {                                                       // se achar, já foi inicializada
        if (aux == (queue_t*)queue)
        {
            found = 1;
            break;
        }
        aux = aux->next;
    }
	if(found)
	{
		current_task->t_class = save;
		return -1;
	}

	queue->next = NULL;                                    // Se não achar, então cria ela
	queue->prev = NULL;
	queue_append(&mqueue,(queue_t*)queue);

	queue->max = max;                                      // Com sua devida inicialização
	queue->size = size;
    queue->first_msg = 0;
    queue->last_msg = 0;
	queue->msgs = (char*)malloc(queue->max*queue->size);

	if (sem_create (&queue->sem_buffer, 1) == -1)
    {
        current_task->t_class = save;
		return -1;
    }
	if (sem_create (&queue->sem_item, 0) == -1)
    {
        current_task->t_class = save;
		return -1;
    }
	if (sem_create (&queue->sem_vaga, max) == -1)
    {
        current_task->t_class = save;
		return -1;
    }

	current_task->t_class = save;
	return 0;
}

// MQUEUE-SEND: Envia uma mensagem para fila, bloqueando a tarefa caso não haja vaga na fila //

int mqueue_send (mqueue_t *queue, void *msg)
{
	if(msg == NULL)                                         // Ponteiro deve ser válido
		return -1;

	task_class save = current_task->t_class;                // Desativa Preempção
	current_task->t_class = System;

    char *msg_buffer = (char*)msg;

    queue_t* aux = (queue_t*)mqueue;
    int found = 0;
    int i;
    for(i = queue_size((queue_t*)mqueue); i > 0; i--)       // Procura a mqueue na fila apropriada para ver se
    {                                                       // essa mqueue ja foi inicializada
        if (aux == (queue_t*)queue)
        {
            found = 1;
            break;
        }
        aux = aux->next;
    }
	if(!found)
	{
		current_task->t_class = save;
		return -1;
	}
	if (sem_down (&queue->sem_vaga) == -1)                   // Esquema de travas de buffer, vaga e item, conforme
    {                                                        // problema clássico produtor / consumidor
        current_task->t_class = save;
		return -1;
    }
	if (sem_down (&queue->sem_buffer) == -1)
    {
        current_task->t_class = save;
		return -1;
    }

    memcpy(queue->msgs + (queue->last_msg*queue->size),msg_buffer,queue->size);
    queue->last_msg = (queue->last_msg + 1) % queue->max;

	if (sem_up (&queue->sem_buffer) == -1)
    {
        current_task->t_class = save;
		return -1;
    }
	if (sem_up (&queue->sem_item) == -1)
    {
        current_task->t_class = save;
		return -1;
    }

	current_task->t_class = save;
	return 0;
}

// MQUEUE RECV: Retira uma mensagem da fila, bloqueando a tarefa caso não haja mensagens //

int mqueue_recv (mqueue_t *queue, void *msg)
{
	if(msg == NULL)                                         // Verifica ponteiro
		return -1;

	task_class save = current_task->t_class;                // Desativa preempções
	current_task->t_class = System;

    char *msg_buffer = (char*)msg;

    queue_t* aux = (queue_t*)mqueue;
    int found = 0;
    int i;
    for(i = queue_size((queue_t*)mqueue); i > 0; i--)
    {
        if (aux == (queue_t*)queue)
        {
            found = 1;                                         // Verifica se a mqueue já foi inicializada
            break;                                             // (deve estar na fila apropriada)
        }
        aux = aux->next;
    }
	if(!found)
	{
		current_task->t_class = save;
		return -1;
	}
	if (sem_down (&queue->sem_item) == -1)                     // Esquema de travas de buffer, vaga e item, conforme
    {                                                          // problema clássico produtor / consumidor
        current_task->t_class = save;
		return -1;
    }
	if (sem_down (&queue->sem_buffer) == -1)
    {
        current_task->t_class = save;
		return -1;
    }

    memcpy(msg_buffer, queue->msgs + (queue->first_msg*queue->size),queue->size);
    queue->first_msg = (queue->first_msg + 1) % queue->max;


	if (sem_up (&queue->sem_buffer) == -1)
    {
        current_task->t_class = save;
		return -1;
    }
	if (sem_up (&queue->sem_vaga) == -1)
    {
        current_task->t_class = save;
		return -1;
    }

	current_task->t_class = save;
	return 0;
}

// MQUEUE DESTROY: Destrói uma fila, liberando todas as tarefas que esperavam para operar sobre ela //

int mqueue_destroy (mqueue_t *queue)
{
	task_class save = current_task->t_class;                        // Desativa preempções
	current_task->t_class = System;

    queue_t* aux = (queue_t*)mqueue;
    int found = 0;
    int i;
    for(i = queue_size((queue_t*)mqueue); i > 0; i--)
    {
        if (aux == (queue_t*)queue)
        {
            found = 1;
            break;
        }
        aux = aux->next;                                    // Procura ela na fila apropriada
    }
	if(!found)
	{
		current_task->t_class = save;
		return -1;
	}

	free(queue->msgs);                                      // Desaloca a mqueue
    queue->first_msg = 0;
    queue->last_msg = 0;

	if (sem_destroy (&queue->sem_buffer) == -1)
    {
    current_task->t_class = save;
		return -1;
    }
	if (sem_destroy (&queue->sem_item) == -1)
    {
        current_task->t_class = save;
		return -1;
    }
	if (sem_destroy (&queue->sem_vaga) == -1)
    {
        current_task->t_class = save;
		return -1;
    }

	current_task->t_class = save;
	return 0;
}

// MQUEUE MSGS: Conta quantas mensagens tem na fila //

int mqueue_msgs (mqueue_t *queue)
{
	task_class save = current_task->t_class;                // Desativa preempções
	current_task->t_class = System;

    queue_t* aux = (queue_t*)mqueue;
    int found = 0;
    int i;
    for(i = queue_size((queue_t*)mqueue); i > 0; i--)
    {
        if (aux == (queue_t*)queue)
        {
            found = 1;                                      // Procura ela na fila apropriada, para ver se foi
            break;                                          // Inicializada
        }
        aux = aux->next;
    }
	if(!found)
	{
		current_task->t_class = save;
		return -1;
	}

    int n_msgs;                                             // Conta a quantidade de mensagens
    if (queue->last_msg < queue->first_msg)
    {
        n_msgs = queue->last_msg + queue->max - queue->first_msg;
    }
    else
    {
        n_msgs = queue->last_msg - queue->first_msg;
    }

	current_task->t_class = save;
	return n_msgs;
}
