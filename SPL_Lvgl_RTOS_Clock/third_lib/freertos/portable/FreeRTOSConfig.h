#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Here is a good place to include header files that are required across
   your application. */
//#include "something.h"

#define configUSE_PREEMPTION                                        1//使用抢占式调度
#define configUSE_PORT_OPTIMISED_TASK_SELECTION                     0//不使用端口优化的任务选择
#define configUSE_TICKLESS_IDLE                                     0//不使用低功耗空闲模式
#define configCPU_CLOCK_HZ                                          168000000ul
// #define configSYSTICK_CLOCK_HZ                                      168000000
#define configTICK_RATE_HZ                                          1000//每1/1000秒产生一次中断
#define configMAX_PRIORITIES                                        10//最大任务优先级数10个，范围0-9
#define configMINIMAL_STACK_SIZE                                    128
#define configMAX_TASK_NAME_LEN                                     16
#define configUSE_16_BIT_TICKS                                      0//使用32位tick类型
#define configIDLE_SHOULD_YIELD                                     1//空闲任务应该让出CPU给其他同优先级的任务
#define configUSE_TASK_NOTIFICATIONS                                1//使能任务通知
#define configTASK_NOTIFICATION_ARRAY_ENTRIES                       3//每个任务的通知数组长度为3
#define configUSE_MUTEXES                                           0//不使用互斥锁
#define configUSE_RECURSIVE_MUTEXES                                 0//不使用递归互斥锁
#define configUSE_COUNTING_SEMAPHORES                               0//不使用计数信号量
#define configUSE_ALTERNATIVE_API                                   0//不使用备用API
//意思是：不使用以 xQueue 为前缀的API函数，而是使用以 xSemaphore 为前缀的API函数来创建和管理信号量
#define configQUEUE_REGISTRY_SIZE                                   10//队列注册表大小为10，允许注册最多10个队列或信号量以供调试使用
#define configUSE_QUEUE_SETS                                        0//不使用队列集合
#define configUSE_TIME_SLICING                                      1//使用时间片调度
#define configUSE_NEWLIB_REENTRANT                                  0//不使用Newlib库的可重入函数
#define configENABLE_BACKWARD_COMPATIBILITY                         0//不启用向后兼容性
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS                     5//每个任务的线程局部存储指针数量为5
#define configUSE_MINI_LIST_ITEM                                    1//使用迷你列表项结构以节省RAM
#define configSTACK_DEPTH_TYPE                                      uint32_t//使用uint32_t类型表示任务栈深度
#define configMESSAGE_BUFFER_LENGTH_TYPE                            size_t//使用size_t类型表示消息缓冲区长度
#define configHEAP_CLEAR_MEMORY_ON_FREE                             1//释放内存时清零以帮助调试

/* Memory allocation related definitions. */
#define configSUPPORT_STATIC_ALLOCATION                             0//不使用静态内存分配
#define configSUPPORT_DYNAMIC_ALLOCATION                            1//使用动态内存分配
#define configTOTAL_HEAP_SIZE                                       1024*30//总堆大小为30KB
#define configAPPLICATION_ALLOCATED_HEAP                            0//不使用应用程序分配的堆
#define configSTACK_ALLOCATION_FROM_SEPARATE_HEAP                   0//不从单独的堆分配任务栈

/* Hook function related definitions. */
#define configUSE_IDLE_HOOK                                 0//空闲钩子函数
#define configUSE_TICK_HOOK                                 0//时钟钩子函数
#define configCHECK_FOR_STACK_OVERFLOW                      1//栈溢出检查
#define configUSE_MALLOC_FAILED_HOOK                        1//堆内分配失败钩子函数
#define configUSE_DAEMON_TASK_STARTUP_HOOK                  0//守护任务启动钩子函数
#define configUSE_SB_COMPLETED_CALLBACK                     0//流/消息缓冲区完成回调函数

/* Run time and task stats gathering related definitions. */
#define configGENERATE_RUN_TIME_STATS                       0//不生成运行时间统计信息
#define configUSE_TRACE_FACILITY                            0
#define configUSE_STATS_FORMATTING_FUNCTIONS                0

/* Co-routine related definitions. */
#define configUSE_CO_ROUTINES                               0
#define configMAX_CO_ROUTINE_PRIORITIES                     1//协程优先级数为1，范围0-0

/* Software timer related definitions. */
#define configUSE_TIMERS                                    1//使能软件定时器
#define configTIMER_TASK_PRIORITY                           8//软件定时器任务优先级
#define configTIMER_QUEUE_LENGTH                            10//软件定时器队列长度
#define configTIMER_TASK_STACK_DEPTH                        configMINIMAL_STACK_SIZE//软件定时器任务栈大小

/* Interrupt nesting behaviour configuration. */
#define configKERNEL_INTERRUPT_PRIORITY                   15<<4
#define configMAX_SYSCALL_INTERRUPT_PRIORITY              5<<4
// NVIC中断优先级 ≥5的中断，才能调用 FreeRTOS 的中断安全 API。因为数值过低优先级太高，临界区中断屏蔽只屏蔽了大于5
#define configMAX_API_CALL_INTERRUPT_PRIORITY             5<<4

/* Define to trap errors during development. */
#define configASSERT( x ) if( ( x ) == 0 ) vAssertCalled( __FILE__, __LINE__ )

/* FreeRTOS MPU specific definitions. */
extern void vAssertCalled(const char *file, int line);
#define configINCLUDE_APPLICATION_DEFINED_PRIVILEGED_FUNCTIONS 0
#define configTOTAL_MPU_REGIONS                                8 /* Default value */
#define configTEX_S_C_B_FLASH                                  0x07UL /* Default value */
#define configTEX_S_C_B_SRAM                                   0x07UL /* Default value */
#define configENFORCE_SYSTEM_CALLS_FROM_KERNEL_ONLY            1
#define configALLOW_UNPRIVILEGED_CRITICAL_SECTIONS             1
#define configENABLE_ERRATA_837070_WORKAROUND                  1

/* ARMv8-M secure side port related definitions. */
#define secureconfigMAX_SECURE_CONTEXTS         5

/* Optional functions - most linkers will remove unused functions anyway. */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     0
#define INCLUDE_uxTaskGetStackHighWaterMark2    0
#define INCLUDE_xTaskGetIdleTaskHandle          0
#define INCLUDE_eTaskGetState                   0
#define INCLUDE_xTimerPendFunctionCall          0
#define INCLUDE_xTaskAbortDelay                 0
#define INCLUDE_xTaskGetHandle                  0
#define INCLUDE_xTaskResumeFromISR              1

/* A header file that defines trace macro can be included here. */

#endif /* FREERTOS_CONFIG_H */
