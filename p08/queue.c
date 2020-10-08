#include "queue.h"
#include <stdio.h>

void queue_append (queue_t **queue, queue_t *elem)
{
   if (queue==NULL)
   {
      fprintf(stderr,"Fila nao existe\n");
      return;
   }
   if (elem==NULL)
   {
      fprintf(stderr,"Elemento nao existe\n");
      return;
   }
   if (elem->next != NULL || elem->prev != NULL)
   {
      fprintf(stderr,"Elemento em outra fila\n");
      return;
   }

   if (*queue == NULL)
   {
      *queue = elem;
      elem->next = elem;
      elem->prev = elem;
   }
   else
   {
   elem->next = *queue;
   elem->prev = (*queue)->prev;

   (*queue)->prev->next = elem;
   (*queue)->prev = elem;
   }
}

queue_t *queue_remove (queue_t **queue, queue_t *elem)
{
   if (queue==NULL)
   {
      fprintf(stderr,"Fila nao existe\n");
      return NULL;
   }
   if (*queue==NULL)
   {
      fprintf(stderr,"Fila vazia\n");
      return NULL;
   }
   if (elem==NULL)
   {
      fprintf(stderr,"Elemento nao existe\n");
      return NULL;
   }

   queue_t* aux = *queue;

   while(aux->next != *queue && aux != elem)
   {
      aux = aux->next;
   }

   if (aux != elem)
   {
      fprintf(stderr,"Elemento nao pertence a fila\n");
      return NULL;
   }

   if (elem == *queue)
   {
      if ((*queue)->next != *queue)
         *queue = elem->next;
      else
         *queue = NULL;
   }

   elem->next->prev = elem->prev;
   elem->prev->next = elem->next;
   elem->next = NULL;
   elem->prev = NULL;
   return elem;
}

int queue_size (queue_t *queue)
{
   int i = 0;
   queue_t* aux = queue;
   if (queue == NULL)
      return 0;

   while (aux->next != queue)
   {
      i++;
      aux = aux->next;
   }
   return i + 1;
}

void queue_print (char *name, queue_t *queue, void print_elem (void*) )
{
   queue_t* ptr = queue;

   printf("%s [",name);

   if (queue != NULL)
   {
      while (ptr->next != queue->prev)
      {
         print_elem(ptr);
         printf(" ");
         ptr = ptr->next;
      }  

      print_elem(ptr);
   }
   
   printf("]\n");
}