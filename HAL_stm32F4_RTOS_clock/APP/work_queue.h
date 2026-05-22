#ifndef WORK_QUEUE_H
#define WORK_QUEUE_H

typedef void (*WorkFunction_t)(void *param);
void work_queue_init(void);
void work_send(WorkFunction_t function, void *param);

#endif /* WORK_QUEUE_H */
