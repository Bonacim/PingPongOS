#include <stdio.h>
#include "queue.h"

void queue_append (queue_t **queue, queue_t *elem)
{
    if(queue != NULL && elem != NULL && elem->next == NULL && elem->prev == NULL)
    {
        if(*queue == NULL)
        {
            *queue = elem;
            elem->next = elem;
            elem->prev = elem;
        }

        else
        {
            elem->prev = (*queue)->prev;
            (*queue)->prev->next = elem;
            elem->next = *queue;
            (*queue)->prev = elem;
        }
    }

    else
    {
        if(queue == NULL)
        {
            printf("\nA fila não existe!\n");
        }

        if(elem == NULL)
        {
            printf("\nO elemento não existe!\n");
        }

        if(elem->next != NULL || elem->prev != NULL)
        {
            printf("\nO elemento já está em outra fila!\n");
        }
    }
}

queue_t *queue_remove (queue_t **queue, queue_t *elem)
{
    if(queue != NULL && *queue != NULL && elem != NULL)
    {
        queue_t *aux = *queue;
        int size = queue_size(*queue);
        int i;

        for(i = 0; i < size; i++)
        {
            if(aux == elem)
            {
                if(size == 1)
                {
                    *queue = NULL;
                }

                else
                {
                    if(aux == *queue)
                    {
                        *queue = aux->next;
                    }

                    aux->prev->next = aux->next;
                    aux->next->prev = aux->prev;
                }

                aux->next = NULL;
                aux->prev = NULL;
                return aux;
            }

            aux = aux->next;
        }
    }

    else
    {
        if(queue == NULL)
        {
            printf("\nA fila não existe!\n");
        }

        if(*queue == NULL)
        {
            printf("\nA fila está vazia!\n");
        }

        if(elem == NULL)
        {
            printf("\nO elemento não existe!\n");
        }
    }

    printf("\nO elemento não pertence a fila!\n");
    return NULL;
}

int queue_size (queue_t *queue)
{
    if(queue == NULL)
    {
        return 0;
    }

    else
    {
        int size = 1;
        queue_t *aux = queue;

        while(aux->next != queue)
        {
            size++;
            aux = aux->next;
        }

        return size;
    }
}

void queue_print (char *name, queue_t *queue, void print_elem (void*))
{
    int size;
    queue_t *aux = queue;

    printf("\n%s", name);
    printf("[");

    for(size = queue_size(queue); size > 0; size--)
    {
        print_elem(aux);
        aux = aux->next;

        if(size != 1)
        {
            printf(" ");
        }
    }

    printf("]");
    printf("\n\n");
}
