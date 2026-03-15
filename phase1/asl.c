#include "./headers/asl.h"
#include "./headers/pcb.h"

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h; // pointer to the head of free semaphore list 
static struct list_head semd_h;     // pointer to the head of ASL 

/*Initialize the semdFree list to contain all the elements of the array static semd_t semdTable[MAXPROC].
This method will be only called once during data structure initialization. */
void initASL() {
    INIT_LIST_HEAD(&semdFree_h);
    INIT_LIST_HEAD(&semd_h);

    for (int i = 0; i < MAXPROC; i++)
    {
        list_add(&(semd_table[i].s_link), &semdFree_h);
    }
}

/*Insert the PCB pointed to by p at the tail of the process queue associated with the semaphore
whose key is semAdd and set the semaphore address of p to semaphore with semAdd. If the
semaphore is currently not active (i.e. there is no descriptor for it in the ASL), allocate a new
descriptor from the semdFree list, insert it in the ASL (at the appropriate position), initialize
all of the fields (i.e. set s_key to semAdd, and s_procq to mkEmptyProcQ()), and proceed as
above. If a new semaphore descriptor needs to be allocated and the semdFree list is empty,
return TRUE. In all other cases return FALSE. */
int insertBlocked(int* semAdd, pcb_t* p) {
    struct semd_t *curr;

    list_for_each_entry(curr, &semd_h, s_link) //search the semaphore whose key = semAdd 
    {
        if (curr->s_key == semAdd){
            list_add_tail(&p->p_list, &curr->s_procq); //insert the PCB pointed to by p at the tail of the process queue 
            p->p_semAdd = semAdd;
            return FALSE;
        }
    }

    if (!(list_empty(&semdFree_h))){  //case for which the semaphore is not in the ASL 
        struct list_head *first = list_next(&semdFree_h);
        struct semd_t *new_sem = container_of(first,semd_t,s_link);
        list_del(first);  //remove it from semdFree_h list
        new_sem->s_key = semAdd;
        mkEmptyProcQ(&new_sem->s_procq);
        list_add_tail(&new_sem->s_link,&semd_h);  //add the new semaphore in the ASL follwing FIFO
        list_add_tail(&p->p_list, &new_sem->s_procq); //insert the PCB pointed to by p at the tail of the process queue 
        p->p_semAdd = semAdd;
        return FALSE;
    }
    return TRUE;
}

/*Search the ASL for a descriptor of this semaphore. If none is found, return NULL; otherwise, remove the first (i.e. head) PCB from the process queue of the found semaphore descriptor and return a pointer to it. If the process queue for this semaphore becomes empty
(emptyProcQ(s_procq) is TRUE), remove the semaphore descriptor from the ASL and return
it to the semdFree list. */
pcb_t* removeBlocked(int* semAdd) {
    struct semd_t *curr;

    list_for_each_entry(curr, &semd_h, s_link) //search the semaphore whose key = semAdd 
    {
        if (curr->s_key == semAdd){
            struct list_head *first = list_next(&curr->s_procq);  //pointer to the first element of the process queue of the found semaphore 
            pcb_t *p = container_of(first,pcb_t,p_list);
        
            list_del(first); //remove first element form the process queue of the semaphore
            if(emptyProcQ(&curr->s_procq)){ //case for which after deleting the element the semaphore become inactive
                list_del(&curr->s_link);
                curr->s_key = NULL;
                list_add(&curr->s_link, &semdFree_h);
            }
            return p;
        }
    }
    return NULL;
}

/*Remove the PCB pointed to by p from the process queue associated with p's semaphore (p->p_semAdd)
on the ASL. If PCB pointed to by p does not appear in the process queue associated with p's
semaphore, which is an error condition, return NULL; otherwise, return p. */
pcb_t* outBlocked(pcb_t* p) {
    struct semd_t *curr;

    list_for_each_entry(curr, &semd_h, s_link) //search the semaphore whose key = p_semAdd
    {
        if (p->p_semAdd == curr->s_key){
            struct list_head *pos;
            list_for_each(pos,&curr->s_procq){  //search p in the process queue of the semaphore
                struct pcb_t *p_curr = container_of(pos,pcb_t,p_list);
                if (p == p_curr){
                    list_del(&p->p_list);
                    if(emptyProcQ(&curr->s_procq)){   //case for which after deleting the element the semaphore become inactive
                        list_del(&curr->s_link); 
                        curr->s_key = NULL;
                        list_add(&curr->s_link, &semdFree_h);
                    }
                    return p;
                }
            }
        }
    }
    return NULL;
}

/*Return a pointer to the PCB that is at the head of the process queue associated with the
semaphore semAdd. Return NULL if semAdd is not found on the ASL or if the process queue
associated with semAdd is empty. */
pcb_t* headBlocked(int* semAdd) {
    struct semd_t *curr;

    list_for_each_entry(curr, &semd_h, s_link)    //search the semaphore whose key = semAdd
    {
        if (curr->s_key == semAdd)
        {
            return headProcQ(&curr->s_procq);
        }
    }
    return NULL;
}
