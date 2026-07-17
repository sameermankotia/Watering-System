/*
 * FreeRTOSConfig.h - minimal-ish config for this project.
 *
 * These are the settings FreeRTOS reads at compile time. I only turned on the
 * stuff I actually use (preemption, queues, semaphores, delays). If you build
 * this for a real MCU you'll probably need to add the port-specific interrupt
 * priority defines at the bottom - grep the FreeRTOS demo for your chip.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#define configUSE_PREEMPTION            1
#define configUSE_IDLE_HOOK             0
#define configUSE_TICK_HOOK             0
#define configCPU_CLOCK_HZ              ( 16000000 )   /* fake, board dependent */
#define configTICK_RATE_HZ             ( 1000 )        /* 1ms tick              */
#define configMAX_PRIORITIES           ( 5 )
#define configMINIMAL_STACK_SIZE       ( 128 )
#define configTOTAL_HEAP_SIZE          ( 10 * 1024 )
#define configMAX_TASK_NAME_LEN        ( 12 )
#define configUSE_16_BIT_TICKS         0
#define configIDLE_SHOULD_YIELD        1

/* features I'm using */
#define configUSE_MUTEXES              1
#define configUSE_COUNTING_SEMAPHORES  1
#define configUSE_TASK_NOTIFICATIONS   1
#define configQUEUE_REGISTRY_SIZE      4

/* memory: heap_4 style dynamic allocation is fine for this */
#define configSUPPORT_DYNAMIC_ALLOCATION  1
#define configSUPPORT_STATIC_ALLOCATION   0

/* which API functions get compiled in - only include what we call */
#define INCLUDE_vTaskDelay             1
#define INCLUDE_vTaskDelayUntil        1
#define INCLUDE_vTaskPrioritySet       0
#define INCLUDE_uxTaskPriorityGet      0
#define INCLUDE_vTaskDelete            1
#define INCLUDE_xTaskGetSchedulerState 1

#endif /* FREERTOS_CONFIG_H */
