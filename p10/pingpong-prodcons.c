#include <stdio.h>
#include <stdlib.h>
#include "pingpong.h"

#define BUFFER_SIZE 5

task_t prod_1, prod_2, prod_3, cons_1, cons_2;
semaphore_t s_buffer, s_item, s_vaga;

void insere_item (int item);
int retira_item ();

int buffer [BUFFER_SIZE];
int itens;
int index;

void produtor (void * arg)
{
    int item;

    while (1)
    {
        task_sleep (1);

        item = random () % 100;
        printf ("%s %d\n", (char *) arg, item);

        sem_down (&s_vaga);

        sem_down (&s_buffer);
        insere_item (item);
        sem_up (&s_buffer);

        sem_up (&s_item);
    }
}

void consumidor (void * arg)
{
    int item;

    while (1)
    {
        sem_down (&s_item);

        sem_down (&s_buffer);
        item = retira_item ();
        sem_up (&s_buffer);

        sem_up (&s_vaga);

        printf ("%s %d\n", (char *) arg, item);

        task_sleep(1);
    }
}

int main (int argc, char *argv[])
{
    printf ("Main INICIO\n");

    for (index = BUFFER_SIZE; index > 0; index--) {
        buffer [index - 1] = -1;
    }

    itens = 0;
    index = 0;

    pingpong_init ();

    sem_create (&s_buffer, 1);
    sem_create (&s_item, 0);
    sem_create (&s_vaga, BUFFER_SIZE);

    task_create (&prod_1, produtor, "P1 produziu");
    task_create (&prod_2, produtor, "P2 produziu");
    task_create (&prod_3, produtor, "P3 produziu");
    task_create (&cons_1, consumidor, "               C1 consumiu");
    task_create (&cons_2, consumidor, "               C2 consumiu");

    task_join (&prod_1);
    task_join (&prod_2);
    task_join (&prod_3);
    task_join (&cons_1);
    task_join (&cons_2);

    sem_destroy (&s_buffer);
    sem_destroy (&s_item);
    sem_destroy (&s_vaga);

    printf ("Main FIM\n");
    task_exit (0);

    exit (0);
}

void insere_item (int i)
{
    if (itens < BUFFER_SIZE)
    {
        buffer [(index + itens) % BUFFER_SIZE] = i;
        itens++;
    }
}

int retira_item ()
{
    int i = -1;

    if (itens > 0)
    {
        i = buffer [index];

        buffer [index] = -1;

        index++;
        index = index % BUFFER_SIZE;

        itens--;
    }

    return i;
}
