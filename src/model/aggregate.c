#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <glib.h>
#include <stdio.h>
#include "aggregate.h"

/*
* Free closest chain
*/
void free_closestchain(struct ClosestTo** head)
{
   struct ClosestTo* tmp;

   while (*head != NULL)
    {
       tmp = *head;
       *head = (*head)->next;
       free(tmp);
    }
}
