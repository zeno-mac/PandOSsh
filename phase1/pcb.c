#include "./headers/pcb.h"

static struct list_head pcbFree_h;
static pcb_t pcbFree_table[MAXPROC];
static int next_pid = 1;

/*Initialize the pcbFree list to contain all the elements of the static array of MAXPROC PCBs.
This method will be called only once during data structure initialization. */
void initPcbs()
{

    INIT_LIST_HEAD(&pcbFree_h);

    for (int i = 0; i < MAXPROC; i++)
    {
        list_add(&(pcbFree_table[i].p_list), &pcbFree_h);
    }
}

// Insert the element pointed to by p onto the pcbFree list.
void freePcb(pcb_t *p)
{
    list_add(&(p->p_list), &pcbFree_h);
}

// Reset all the values of the pcb
void initPcb(pcb_t *p)
{

    INIT_LIST_HEAD(&p->p_list);
    p->p_parent = NULL;
    INIT_LIST_HEAD(&p->p_child);
    INIT_LIST_HEAD(&p->p_sib);
    p->p_s.entry_hi = 0;
    p->p_s.cause = 0;
    p->p_s.status = 0;
    p->p_s.pc_epc = 0;
    p->p_s.mie = 0;
    for (int i = 0; i < STATE_GPR_LEN; i++)
    {
        p->p_s.gpr[i] = 0;
    }
    p->p_semAdd = NULL;
    p->p_supportStruct = NULL;
    p->p_time = 0;

    p->p_prio = 0;
    p->p_pid = next_pid;
    next_pid++;
}

/*Return NULL if the pcbFree list is empty. Otherwise, remove an element from the pcbFree
list, provide initial values for ALL of the PCBs fields (i.e. NULL and/or 0), except for
p_pid which is incremented each time, and then return a pointer to the removed element.
PCBs get reused, so it is important that no previous value persist in a PCB when it gets
reallocated*/
pcb_t *allocPcb()
{
    struct list_head *list = list_next(&pcbFree_h);
    if (!list)
    {
        return NULL;
    }
    else
    {
        list_del(list);
    }

    pcb_t *p = container_of(list, pcb_t, p_list);
    initPcb(p);
    return p;
}

/*Initialize a variable to be head pointer to a process queue.*/
void mkEmptyProcQ(struct list_head *head)
{
    if (head)
    {
        INIT_LIST_HEAD(head);
    }
}

/*Return TRUE if the queue whose head is pointed to by head is empty. Return FALSE otherwise.*/
int emptyProcQ(struct list_head *head)
{
    return list_empty(head);
}

/*Insert the PCB pointed by p into the process queue whose head pointer is pointed to by head.
The list must be ordered by priority. In case of equal priority, the new PCB must be inserted
after the last PCB with this priority. For example, if you insert PCB N with priority 10 to the
queue, A(20), B(10), C(0), the list becomes A(20), B(10), N(10), C(0).*/
void insertProcQ(struct list_head *head, pcb_t *p)
{
    struct list_head *curr;
    int pPrio = p->p_prio;
    list_for_each(curr, head)
    {
        int currPrio = container_of(curr, pcb_t, p_list)->p_prio;
        if (currPrio < pPrio)
            break;
    }
    list_add(&(p->p_list), curr->prev);
}

/*Return a pointer to the first PCB (i.e. the PCB with max priority) from the process queue
whose head is pointed to by head. Do not remove this PCB from the process queue.
Return NULL if the process queue is empty*/
pcb_t *headProcQ(struct list_head *head)
{
    if (list_empty(head))
        return NULL;
    return container_of(list_next(head), pcb_t, p_list);
}

/*Remove a pointer to the first PCB (i.e. the PCB with max priority) from the process queue
whose head is pointed to by head. Return NULL if the process queue was initially empty;
otherwise return the pointer to the removed element*/
pcb_t *removeProcQ(struct list_head *head)
{
    if (list_empty(head))
        return NULL;
    pcb_t *retVal = container_of(list_next(head), pcb_t, p_list);
    list_del(head->next);
    return retVal;
}

/*Remove the PCB pointed to by p from the process queue whose head pointer is pointed to by
head. If the desired entry is not in the indicated queue (an error condition), return NULL;
otherwise, return p. Note that p can point to any element of the process queue.*/
pcb_t *outProcQ(struct list_head *head, pcb_t *p)
{
    struct list_head *curr = list_next(head);
    list_for_each(curr, head)
    {
        if (p == container_of(curr, pcb_t, p_list))
        {
            // devo rimuovere curr e ritornare p
            list_del(curr);
            return p;
        }
    }
    return NULL;
}

/*Return TRUE if the PCB pointed to by p has no children. Return FALSE otherwise.*/
int emptyChild(pcb_t *p)
{
    return (!p || list_empty(&p->p_child));
}

/*Make the PCB pointed to by p a child of the PCB pointed to by prnt.*/
void insertChild(pcb_t *prnt, pcb_t *p)
{
    if (!prnt || !p)
    {
        return;
    }
    p->p_parent = prnt;
    list_add(&p->p_sib, &prnt->p_child);
}

/*Make the first child of the PCB pointed to by p no longer a child of p. Return NULL if initially
there were no children of p. Otherwise, return a pointer to this removed first child PCB.*/
pcb_t *removeChild(pcb_t *p)
{
    if (emptyChild(p))
    {
        return NULL;
    }
    struct list_head *child_list = list_next(&p->p_child);
    list_del(child_list);
    pcb_t *child = container_of(child_list, pcb_t, p_sib);
    child->p_parent = NULL;
    return child;
}

/*Make the PCB pointed to by p no longer the child of its parent. If the PCB pointed to by p has
no parent, return NULL; otherwise, return p. Note that the element pointed to by p could be
in an arbitrary position (i.e. not be the first child of its parent).*/
pcb_t *outChild(pcb_t *p)
{
    if (!p || !p->p_parent)
    {
        return NULL;
    }
    list_del(&p->p_sib);
    p->p_parent = NULL;
    return p;
}
