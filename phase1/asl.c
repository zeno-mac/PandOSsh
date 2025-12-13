#include "./headers/asl.h"

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h; // pointer to free semaphore list 
static struct list_head semd_h;     // pointer to ASL 

void initASL() {
    INIT_LIST_HEAD(&semdFree_h);
    INIT_LIST_HEAD(&semd_h);

    for (int i = 0; i < MAXPROC; i++)
    {
        list_add(&(semd_table[i].s_link), &semdFree_h);
    }
}

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
                        curr->s_key = NULL; //aggiunto ma non necessario teoricamente
                        list_add(&curr->s_link, &semdFree_h);
                    }
                    return p;
                }
            }
        }
    }
    return NULL;
}

pcb_t* headBlocked(int* semAdd) {
    struct semd_t *curr;

    list_for_each_entry(curr, &semd_h, s_link)    //search the semaphore whose key = semAdd
    {
        if (curr->s_key == semAdd)
        {
            //return headProcQ(&curr->s_procq);
            return container_of(list_next(&curr->s_procq),pcb_t,p_list);//yesh
        }
    }
    return NULL;
}
