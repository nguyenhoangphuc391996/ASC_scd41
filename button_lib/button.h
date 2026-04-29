/*
 * button.h
 *
 *  Created on: Apr 29, 2026
 *      Author: embedded
 */

#ifndef BUTTON_H_
#define BUTTON_H_

#include "main.h"

typedef enum
{
    BUTTON_EVENT_NONE = 0,
    BUTTON_EVENT_SHORT,
    BUTTON_EVENT_LONG
} ButtonEvent_t;

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    GPIO_PinState active_state;
    uint32_t debounce_ms;
    uint32_t long_press_ms;

    GPIO_PinState last_raw_state;
    GPIO_PinState stable_state;
    uint32_t last_change_ms;
    uint32_t press_start_ms;
    uint8_t long_event_sent;
    uint8_t initialized;
} ButtonHandle_t;

void Button_Init(ButtonHandle_t *btn,
                 GPIO_TypeDef *port,
                 uint16_t pin,
                 GPIO_PinState active_state,
                 uint32_t debounce_ms,
                 uint32_t long_press_ms);

ButtonEvent_t Button_Update(ButtonHandle_t *btn);

#endif /* BUTTON_H_ */