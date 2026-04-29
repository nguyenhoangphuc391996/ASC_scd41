/*
 * button.c
 *
 *  Created on: Apr 29, 2026
 *      Author: embedded
 */

#include "button.h"

static GPIO_PinState Button_ReadRaw(const ButtonHandle_t *btn)
{
	return HAL_GPIO_ReadPin(btn->port, btn->pin);
}

void Button_Init(ButtonHandle_t *btn,
				 GPIO_TypeDef *port,
				 uint16_t pin,
				 GPIO_PinState active_state,
				 uint32_t debounce_ms,
				 uint32_t long_press_ms)
{
	if (btn == NULL)
	{
		return;
	}

	btn->port = port;
	btn->pin = pin;
	btn->active_state = active_state;
	btn->debounce_ms = debounce_ms;
	btn->long_press_ms = long_press_ms;
	btn->last_raw_state = Button_ReadRaw(btn);
	btn->stable_state = btn->last_raw_state;
	btn->last_change_ms = HAL_GetTick();
	btn->press_start_ms = btn->last_change_ms;
	btn->long_event_sent = 0;
	btn->initialized = 1;
}

ButtonEvent_t Button_Update(ButtonHandle_t *btn)
{
	GPIO_PinState raw_state;
	uint32_t now;

	if ((btn == NULL) || (btn->initialized == 0U))
	{
		return BUTTON_EVENT_NONE;
	}

	now = HAL_GetTick();
	raw_state = Button_ReadRaw(btn);

	if (raw_state != btn->last_raw_state)
	{
		btn->last_raw_state = raw_state;
		btn->last_change_ms = now;
	}

	if ((now - btn->last_change_ms) < btn->debounce_ms)
	{
		return BUTTON_EVENT_NONE;
	}

	if (raw_state != btn->stable_state)
	{
		btn->stable_state = raw_state;

		if (btn->stable_state == btn->active_state)
		{
			btn->press_start_ms = now;
			btn->long_event_sent = 0U;
		}
		else
		{
			if ((now - btn->press_start_ms) >= btn->long_press_ms)
			{
				btn->long_event_sent = 1U;
				return BUTTON_EVENT_LONG;
			}

			btn->long_event_sent = 0U;
			return BUTTON_EVENT_SHORT;
		}
	}

	return BUTTON_EVENT_NONE;
}