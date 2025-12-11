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

    list_for_each_entry(curr, &semd_h, s_link)
    {

        if (curr->s_key == semAdd)
        {
            insertProcQ(&curr->s_procq, p); //non bisogna usare funzioni all'esterno di asl.c
            p->p_semAdd = semAdd;
            return FALSE;
        }
    }

    if (!(list_empty(&semdFree_h)))
    {
        struct list_head *ptr = list_next(&semdFree_h);
        list_del(ptr);

        // ptr è il puntatore al campo s_link, io voglio il puntatore a semd_t
        struct semd_t *newsem = container_of(ptr, semd_t, s_link);

        newsem->s_key = semAdd;
        mkEmptyProcQ(&newsem->s_procq);//non bisogna usare funzioni all'esterno di asl.c
        list_add(&newsem->s_link, &semd_h);

        p->p_semAdd = semAdd;
        insertProcQ(&newsem->s_procq, p);//non bisogna usare funzioni all'esterno di asl.c
        return FALSE;
    }
    return TRUE;
}

pcb_t* removeBlocked(int* semAdd) {
}

pcb_t* outBlocked(pcb_t* p) {
}

pcb_t* headBlocked(int* semAdd) {
    struct semd_t *curr;

    list_for_each_entry(curr, &semd_h, s_link)
    {
        if (curr->s_key == semAdd)
        {
            return headProcQ(&curr->s_procq);//non bisogna usare funzioni all'esterno di asl.c
        }
    }
    return NULL;
}
