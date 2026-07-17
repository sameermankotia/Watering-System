/*
 * main.c - Smart Plant Moisture Control System
 *
 * FreeRTOS project. Three tasks:
 *   - SensorTask     reads the soil moisture, sends it on a queue
 *   - ControllerTask reads the queue, runs a state machine, drives the pump
 *   - StatusTask     heartbeat led + prints what's going on
 *
 * Plus a button interrupt that lets you force a watering by hand. The ISR
 * "gives" a binary semaphore and the controller "takes" it.
 *
 * Concepts I was trying to hit: tasks, priorities, a queue for data, a
 * semaphore for the isr->task signal, and a state machine. See README.
 */
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "hal.h"

/* ---- tuning knobs -------------------------------------------------------- */

#define MOISTURE_DRY      30    /* below this % we start watering            */
#define MOISTURE_WET      60    /* stop once we get back up to this %        */

#define WATER_MS          3000  /* max time to run the pump in one go (ms)   */
#define SOAK_MS           5000  /* wait after watering so water spreads      */
#define SENSOR_PERIOD_MS  1000  /* how often we read the sensor              */

/* stop the button spamming - ignore presses within this window */
#define BUTTON_COOLDOWN_MS 2000

/* ---- state machine ------------------------------------------------------- */

typedef enum {
    STATE_IDLE = 0,   /* soil is fine, doing nothing                         */
    STATE_WATERING,   /* pump is on, filling up the soil                     */
    STATE_SOAKING     /* pump off, waiting for water to soak in before re-checking */
} pump_state_t;

static const char *state_name(pump_state_t s)
{
    switch (s) {
        case STATE_IDLE:     return "IDLE";
        case STATE_WATERING: return "WATERING";
        case STATE_SOAKING:  return "SOAKING";
        default:             return "???";
    }
}

/* ---- shared handles ------------------------------------------------------ */

static QueueHandle_t     moistureQueue;   /* SensorTask -> ControllerTask     */
static SemaphoreHandle_t manualButtonSem; /* button ISR -> ControllerTask     */

/* controller writes this so StatusTask can print it. only the controller
 * writes it, everyone else just reads, so I'm not bothering with a lock. */
static volatile pump_state_t g_state = STATE_IDLE;
static volatile uint8_t      g_last_moisture = 0;

/* ---- the button interrupt ------------------------------------------------ */

/*
 * This runs in interrupt context, so keep it SHORT. No printf, no delays.
 * All it does is give the semaphore to wake up the controller. We also do a
 * cheap cooldown so a bouncy button doesn't fire 10 times.
 *
 * xHigherPriorityTaskWoken lets FreeRTOS switch straight to the controller
 * when we exit the isr, instead of waiting for the next tick.
 */
static void button_isr(void)
{
    static TickType_t last_press = 0;
    BaseType_t higher_woken = pdFALSE;

    TickType_t now = xTaskGetTickCountFromISR();
    if ((now - last_press) < pdMS_TO_TICKS(BUTTON_COOLDOWN_MS)) {
        return;  /* too soon, ignore (crude debounce) */
    }
    last_press = now;

    xSemaphoreGiveFromISR(manualButtonSem, &higher_woken);
    portYIELD_FROM_ISR(higher_woken);
}

/* ---- tasks --------------------------------------------------------------- */

/* read the sensor on a fixed period and drop the value on the queue */
static void SensorTask(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        uint8_t moisture = hal_read_moisture();

        /* if the queue is full just overwrite-ish; here we wait a tiny bit.
         * queue is length 1 so newest reading basically wins. */
        xQueueSend(moistureQueue, &moisture, pdMS_TO_TICKS(10));

        /* vTaskDelayUntil keeps the period steady even if the send took a while */
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SENSOR_PERIOD_MS));
    }
}

/*
 * The brain. Waits for a moisture reading OR a button press, then decides
 * what the pump should do. This is the state machine.
 */
static void ControllerTask(void *arg)
{
    (void)arg;
    pump_state_t state = STATE_IDLE;
    uint8_t moisture = 50;
    TickType_t water_start = 0;   /* when we turned the pump on   */
    TickType_t soak_start  = 0;   /* when we started soaking      */

    for (;;) {
        /* --- 1. check for a manual button press (non-blocking) --- */
        if (xSemaphoreTake(manualButtonSem, 0) == pdTRUE) {
            printf("[CTRL] manual watering requested\n");
            /* force us into watering no matter the reading */
            if (state == STATE_IDLE) {
                state = STATE_WATERING;
                hal_pump_on();
                water_start = xTaskGetTickCount();
            }
        }

        /* --- 2. get the latest moisture reading --- */
        if (xQueueReceive(moistureQueue, &moisture, pdMS_TO_TICKS(200)) == pdTRUE) {
            g_last_moisture = moisture;
        }

        /* --- 3. run the state machine --- */
        switch (state) {

        case STATE_IDLE:
            /* dried out? start watering. */
            if (moisture < MOISTURE_DRY) {
                printf("[CTRL] soil dry (%u%%), watering\n", moisture);
                hal_pump_on();
                water_start = xTaskGetTickCount();
                state = STATE_WATERING;
            }
            break;

        case STATE_WATERING: {
            TickType_t elapsed = xTaskGetTickCount() - water_start;

            /* stop if we hit the target OR we've run the pump too long.
             * the max-time check is a safety thing so we never flood it. */
            if (moisture >= MOISTURE_WET) {
                printf("[CTRL] target reached (%u%%), soaking\n", moisture);
                hal_pump_off();
                soak_start = xTaskGetTickCount();
                state = STATE_SOAKING;
            } else if (elapsed >= pdMS_TO_TICKS(WATER_MS)) {
                printf("[CTRL] pump ran max time, soaking to be safe\n");
                hal_pump_off();
                soak_start = xTaskGetTickCount();
                state = STATE_SOAKING;
            }
            break;
        }

        case STATE_SOAKING: {
            /* let the water spread out before we trust the sensor again,
             * otherwise the top stays wet and we under-water. */
            TickType_t elapsed = xTaskGetTickCount() - soak_start;
            if (elapsed >= pdMS_TO_TICKS(SOAK_MS)) {
                printf("[CTRL] done soaking, back to idle\n");
                state = STATE_IDLE;
            }
            break;
        }

        default:
            /* shouldn't happen, but if it does, pump off + reset */
            hal_pump_off();
            state = STATE_IDLE;
            break;
        }

        g_state = state;  /* let StatusTask see where we are */
    }
}

/* low priority: blink led + print a status line so we know it's alive */
static void StatusTask(void *arg)
{
    (void)arg;
    for (;;) {
        hal_led_toggle();
        printf("[STAT] state=%-8s moisture=%u%%\n",
               state_name(g_state), g_last_moisture);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ---- boot ---------------------------------------------------------------- */

int main(void)
{
    hal_init();

    /* queue holds 1 moisture sample (a byte). length 1 = we only care about
     * the most recent reading. */
    moistureQueue = xQueueCreate(1, sizeof(uint8_t));

    /* binary semaphore for the button. starts empty (not given). */
    manualButtonSem = xSemaphoreCreateBinary();

    if (moistureQueue == NULL || manualButtonSem == NULL) {
        printf("[BOOT] failed to create queue/semaphore, out of heap?\n");
        for (;;) { }
    }

    /* wire up the button interrupt now that the semaphore exists */
    hal_register_button_isr(button_isr);

    /* create tasks. args: fn, name, stack words, param, priority, handle.
     * controller is highest so it reacts fast, status is lowest. */
    xTaskCreate(SensorTask,     "sensor", 256, NULL, 2, NULL);
    xTaskCreate(ControllerTask, "ctrl",   256, NULL, 3, NULL);
    xTaskCreate(StatusTask,     "status", 256, NULL, 1, NULL);

    printf("[BOOT] starting scheduler\n");
    vTaskStartScheduler();

    /* only get here if the scheduler ran out of heap and returned */
    printf("[BOOT] scheduler returned - out of memory\n");
    for (;;) { }
    return 0;
}
