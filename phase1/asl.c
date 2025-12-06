#include "./headers/asl.h"

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h; // puntatore lista inattivi
static struct list_head semd_h;     // puntatore ASL

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

    if (!(list_empty(&semdFree_h))){
        struct list_head *first = semdFree_h.next;
        struct semd_t *new_sem = container_of(first,semd_t,s_link);
        list_del(first);
        new_sem->s_key = semAdd;
        INIT_LIST_HEAD(&new_sem->s_procq); //same as mkEmptyProcQ()   
        list_add(&new_sem->s_link, &semd_h); //non so cosa significa (at the appropriate position)
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
            struct list_head *first = curr->s_procq.next;
            pcb_t *p = container_of(first,pcb_t,p_list);
        
            list_del(first); //remove first element form s_procq
            if(list_empty(&curr->s_procq)){
                list_del(&curr->s_link);
                list_add(&curr->s_link, &semdFree_h);
            }
            return p;
        }
    }
    return NULL;
}

pcb_t* outBlocked(pcb_t* p) {
    struct semd_t *curr;

    list_for_each_entry(curr, &semd_h, s_link) //search the semaphore whose key = semAdd 
    {
        if (p->p_semAdd == curr->s_key){
            struct list_head *pos;
            list_for_each(pos,&curr->s_procq){
                struct pcb_t *p_curr = container_of(pos,pcb_t,p_list);
                if (p == p_curr){
                    list_del(&p->p_list);
                    if(list_empty(&curr->s_procq)){
                        list_del(&curr->s_link); 
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

    list_for_each_entry(curr, &semd_h, s_link)
    {
        if (curr->s_key == semAdd)
        {
            struct pcb_t *p_head = container_of(curr->s_procq.next,pcb_t,p_list);
            return (p_head);
        }
    }
    return NULL;
}
