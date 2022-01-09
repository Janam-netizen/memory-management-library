#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"
//#include <stddef.h>
#include "memory_management.h"

/*TO D0:
OPTIMISE THE PROCEDURE FOR EXTENDING THE HEAP WHEN NO FREE MEMORY IS FOUND.
*/


#define PG 4096
//BELOW CODE TAKEN FROM kernel/riscv.h
#define PGROUND(sz) (((sz) + PG - 1) & ~(PG - 1))


struct meta_data
{

  int free;

  int size;
  struct meta_data *next;

  struct meta_data *prev;
};


static struct meta_data *head=0;


int align(int nbytes)
{

  if (nbytes % 8 != 0)
  {

    nbytes = (int)((int)(nbytes / 8) * 8 + 8);
  }

  return nbytes;
}

struct meta_data *find_last_block()
{

  struct meta_data *curr_block = head;

  while (curr_block->next != 0)
  {

    curr_block = curr_block->next;
  }

  return curr_block;
}

void merge(struct meta_data *header, struct meta_data *front_addr)
{

  if (header != 0)
  {
    //header->free = 1;

    if (front_addr != 0 && front_addr->free == 1 && header->free == 1)
    {

      if (front_addr->next != 0)
      {

        front_addr->next->prev = header;
      }

      header->next = front_addr->next;

      header->size += front_addr->size + sizeof(struct meta_data);
    }
  }
}

void split_chunk(struct meta_data *block, int required_size)
{
  if (block->size > (required_size + sizeof(struct meta_data)))
  {

    //fprintf(1, "splitting block\n");

    struct meta_data *new_block = (struct meta_data *)((unsigned long)block + sizeof(struct meta_data) + (unsigned long)required_size);

    //fprintf(1, "%p\n", new_block);

    new_block->prev = block;

    //fprintf(1, "connecting two blocks\n");

    new_block->next = block->next;

    block->next = new_block;

    new_block->size = block->size - required_size - sizeof(struct meta_data);
    new_block->free = 1;

    block->size = required_size;

    //fprintf(1, "Splitting done\n");
  }
}

void *first_fit(int n)
{

  struct meta_data *curr_block = head;

  while (curr_block != 0)
  { //Searching for a memory block within linked list.

    if (curr_block->free == 1 && curr_block->size >= n)
    {

      curr_block->free = 0;
      split_chunk(curr_block, n);
      return curr_block + sizeof(struct meta_data);
    }

    curr_block = curr_block->next;
  }

  return 0;
}

struct meta_data *find_closest_fit(int n)
{
  struct meta_data *curr_block = head;
  struct meta_data *res = 0;

  int min_size_diff = -1;
  int size_dif = 0;
  while (curr_block != 0)
  { //Searching for a memory block within linked list.

    if (curr_block->free == 1 && curr_block->size >= n)
    {
      size_dif = curr_block->size - n;

      if (min_size_diff == -1 || (size_dif < min_size_diff && size_dif >= 0))
      {
        res = curr_block;

        min_size_diff = size_dif;
      }
    }

    curr_block = curr_block->next;

    min_size_diff += 0;
  }

  if (res != 0)
  {

    res->free = 0;
  }

  return res;
}


void *
_malloc(int nbytes)
{
  //void *p;

  nbytes = align(nbytes);

  if (!head)
  { // No dynamic memory allocation has taken place.
    //fprintf(1, "NO MEMMORY ALLOCATION HAS TAKEN PLACE\n");
    head = (struct meta_data *)sbrk(PGROUND(nbytes + sizeof(struct meta_data)));
    head->free = 0;
    head->size = PGROUND(nbytes + sizeof(struct meta_data)) - sizeof(struct meta_data);
    head->next = 0;

    head->prev = 0;
    split_chunk(head, nbytes);

    return head + sizeof(struct meta_data);
  }

  else
  {

    struct meta_data *closest_fit = find_closest_fit(nbytes);

    if (closest_fit != 0)
    {

      split_chunk(closest_fit, nbytes);

      return closest_fit + sizeof(struct meta_data);
    }

    //If no memory block found ,then extend the heap

    //fprintf(1, "EXTENDING HEAP SINCE NO FREE MEMORY FOUND\n");

    //fprintf(1, "NO OF BYTES: %d\n", nbytes);

    struct meta_data *new_block = (struct meta_data *)sbrk(PGROUND(sizeof(struct meta_data) + nbytes));

    new_block->free = 1;
    new_block->size = PGROUND(sizeof(struct meta_data) + nbytes)-sizeof(struct meta_data);
    new_block->next = 0;

    struct meta_data *last_block = find_last_block();

    if (last_block->free == 1)
    {

      merge(last_block, new_block);
      split_chunk(last_block, nbytes);
      last_block->free = 0;
      return last_block + sizeof(struct meta_data);
    }
    else
    {
      last_block->next = new_block;
      new_block->prev = last_block;
      new_block->next = 0;

      split_chunk(new_block, nbytes);
    }

    return new_block + sizeof(struct meta_data);
  }
}

/*void print_block(struct meta_data *curr_block)
{

  fprintf(1, "%p\n", curr_block);
  fprintf(1, "SIZE:%d\n", curr_block->size);
  fprintf(1, "FREE:%d\n", curr_block->free);
  fprintf(1, "PRE-ADDR:%p\n", curr_block->prev);
  fprintf(1, "NEXT-ADDR:%p\n", curr_block->next);
  fprintf(1, "--------------------\n");
}

void scan_free_space()
{

  struct meta_data *curr_block = head;

  while (curr_block != 0)
  {
    fprintf(1, "%p\n", curr_block);
    fprintf(1, "SIZE:%d\n", curr_block->size);
    fprintf(1, "FREE:%d\n", curr_block->free);
    fprintf(1, "PRE-ADDR:%p\n", curr_block->prev);
    fprintf(1, "NEXT-ADDR:%p\n", curr_block->next);
    fprintf(1, "--------------------\n");

    curr_block = curr_block->next;
  }
}*/

void _free(void *p)
{
  struct meta_data *header = (struct meta_data *)p - sizeof(struct meta_data);

  header->free = 1;

  struct meta_data *back_addr = header->prev;

  struct meta_data *front_addr = header->next;

  merge(header, front_addr);

  merge(back_addr, header);

  //FREE UP SPACE IF LAST BLOCK HAS MORE THAN 4096 BYTES USING PGROUND

  struct meta_data *last_block = find_last_block();
  int reduce=last_block->size+sizeof(struct meta_data);

  if (reduce>=4096 && last_block->free == 1)
  {

    if(reduce %4096!=0){

    reduce=PGROUND(reduce)-4096;
    }

    reduce*=-1;
    sbrk(reduce);

    last_block->size+=reduce;

    
  }
}

