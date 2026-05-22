#ifndef __WORK_QUEUE_H__
#define __WORK_QUEUE_H__

typedef void (*work_func_t)(void*);

void work_queue_init(void);
void work_queue_send(work_func_t work, void* param);

#endif // __WORK_QUEUE_H__
