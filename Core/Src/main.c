/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "itm.h"
#include "button.h"
#include "scd4x_i2c.h"
#include <stdbool.h>

#undef Error_Handler
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

/* Definitions for TaskASC */
osThreadId_t TaskASCHandle;
const osThreadAttr_t TaskASC_attributes = {
  .name = "TaskASC",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for MutexI2C1 */
osMutexId_t MutexI2C1Handle;
const osMutexAttr_t MutexI2C1_attributes = {
  .name = "MutexI2C1"
};
/* USER CODE BEGIN PV */
typedef enum {
  LED_MODE_OFF = 0,
  LED_MODE_BLINK_SLOW,
  LED_MODE_BLINK_FAST,
  LED_MODE_ON
} LedMode_t;

typedef struct {
  scd4x_runtime_t runtime;
  bool sensor_connected;
  bool asc_enabled;
  bool measurement_running;
  bool measurement_has_data;
  LedMode_t led_mode;
  uint32_t last_led_toggle_ms;
  uint32_t last_link_check_ms;
} AppState_t;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
void StartTaskASC(void *argument);

/* USER CODE BEGIN PFP */
static void LED_SetOn(void);
static void LED_SetOff(void);
static void LED_ApplyMode(AppState_t *app, uint32_t now_ms);
static int16_t Sensor_CheckConnection(void);
static int16_t Sensor_ReadAscState(uint16_t *asc_enabled_raw);
static int16_t Sensor_StopMeasurement(void);
static int16_t Sensor_StartMeasurement(AppState_t *app);
static int16_t Sensor_SetAscAndPersist(uint16_t asc_enabled);
static void App_UpdateLedMode(AppState_t *app);
static void App_HandleShortPress(AppState_t *app);
static void App_HandleLongPress(AppState_t *app);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void LED_SetOn(void)
{
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
}

static void LED_SetOff(void)
{
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
}

static void LED_ApplyMode(AppState_t *app, uint32_t now_ms)
{
  if (app->led_mode == LED_MODE_OFF)
  {
    LED_SetOff();
    return;
  }

  if (app->led_mode == LED_MODE_ON)
  {
    LED_SetOn();
    return;
  }

  if (app->led_mode == LED_MODE_BLINK_SLOW)
  {
    if ((now_ms - app->last_led_toggle_ms) >= 500U)
    {
      app->last_led_toggle_ms = now_ms;
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    }
    return;
  }

  if ((now_ms - app->last_led_toggle_ms) >= 100U)
  {
    app->last_led_toggle_ms = now_ms;
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
  }
}

static int16_t Sensor_CheckConnection(void)
{
  uint16_t data_ready_raw = 0U;
  return scd4x_get_data_ready_status_raw(&data_ready_raw);
}

static int16_t Sensor_ReadAscState(uint16_t *asc_enabled_raw)
{
  if (asc_enabled_raw == NULL)
  {
    return NOT_IMPLEMENTED_ERROR;
  }

  /* Do not stop measurement here; boot path must be non-intrusive. */
  return scd4x_get_automatic_self_calibration_enabled(asc_enabled_raw);
}

static int16_t Sensor_StopMeasurement(void)
{
  int16_t err = scd4x_stop_periodic_measurement();
  if (err == NO_ERROR)
  {
  }
  return err;
}

static int16_t Sensor_StartMeasurement(AppState_t *app)
{
  int16_t err = scd4x_runtime_start_periodic_measurement(&app->runtime);
  if (err == NO_ERROR)
  {
    app->measurement_running = true;
    app->measurement_has_data = false;
  }
  return err;
}

static int16_t Sensor_SetAscAndPersist(uint16_t asc_enabled)
{
  int16_t err = scd4x_set_automatic_self_calibration_enabled(asc_enabled);
  if (err != NO_ERROR)
  {
    return err;
  }

  err = scd4x_persist_settings();
  if (err == NO_ERROR)
  {
  }
  return err;
}

static void App_UpdateLedMode(AppState_t *app)
{
  if (!app->sensor_connected)
  {
    app->led_mode = LED_MODE_OFF;
    return;
  }

  if (!app->asc_enabled)
  {
    app->led_mode = LED_MODE_BLINK_SLOW;
    return;
  }

  if (!app->measurement_has_data)
  {
    app->led_mode = LED_MODE_BLINK_FAST;
    return;
  }

  app->led_mode = LED_MODE_ON;
}

static void App_HandleShortPress(AppState_t *app)
{
  int16_t err;

  if (!app->sensor_connected)
  {
    itm_print("[APP] SHORT ignored: sensor disconnected\r\n");
    return;
  }

  if (!app->asc_enabled)
  {
    (void)Sensor_StopMeasurement();

    err = Sensor_SetAscAndPersist(1U);
    if (err != NO_ERROR)
    {
      itm_print("[APP] SHORT failed: enable ASC\r\n");
      return;
    }

    app->asc_enabled = true;
    (void)Sensor_StartMeasurement(app);
    return;
  }

  if (!app->measurement_has_data)
  {
    (void)Sensor_StartMeasurement(app);
    return;
  }

  itm_print("[APP] SHORT ignored: measuring already stable\r\n");
}

static void App_HandleLongPress(AppState_t *app)
{
  int16_t err;

  if (!app->sensor_connected)
  {
    itm_print("[APP] LONG ignored: sensor disconnected\r\n");
    return;
  }

  (void)Sensor_StopMeasurement();

  err = Sensor_SetAscAndPersist(0U);
  if (err != NO_ERROR)
  {
    itm_print("[APP] LONG failed: disable ASC\r\n");
    return;
  }

  app->asc_enabled = false;
  app->measurement_running = false;
  app->measurement_has_data = false;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();
  /* Create the mutex(es) */
  /* creation of MutexI2C1 */
  MutexI2C1Handle = osMutexNew(&MutexI2C1_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of TaskASC */
  TaskASCHandle = osThreadNew(StartTaskASC, NULL, &TaskASC_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PB12 */
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartTaskASC */
/**
  * @brief  Function implementing the TaskASC thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartTaskASC */
void StartTaskASC(void *argument)
{
  /* USER CODE BEGIN 5 */

  int16_t err;
  ButtonHandle_t g_button1;
  AppState_t app = {0};
  uint16_t asc_enabled_raw = 0U;

  scd4x_runtime_init(&app.runtime, &hi2c1, MutexI2C1Handle);
  scd4x_runtime_set_event_callback(&app.runtime,
                                   scd4x_runtime_default_itm_event_handler,
                                   NULL);

  err = Sensor_CheckConnection();
  app.sensor_connected = (err == NO_ERROR);

  if (app.sensor_connected)
  {
    err = Sensor_ReadAscState(&asc_enabled_raw);
    if (err == NO_ERROR)
    {
      app.asc_enabled = (asc_enabled_raw != 0U);
    }
    else
    {
      /* Sensor may already be in periodic mode after MCU reboot: do not stop it. */
      app.asc_enabled = true;
      app.measurement_running = true;
      app.measurement_has_data = true;
      itm_print("[APP] boot: sensor already measuring, skip stop/start\r\n");
    }
  }

  if (app.sensor_connected && app.asc_enabled && !app.measurement_running)
  {
    err = Sensor_StartMeasurement(&app);
    if (err != NO_ERROR)
    {
      app.measurement_running = false;
      app.measurement_has_data = false;
    }
  }

  app.last_led_toggle_ms = HAL_GetTick();
  app.last_link_check_ms = HAL_GetTick();

  Button_Init(&g_button1,
              GPIOB,
              GPIO_PIN_12,
              GPIO_PIN_RESET,
              30U,
              2000U);

  /* Infinite loop */
  for(;;)
  {
    uint32_t now_ms = HAL_GetTick();
    ButtonEvent_t btn_event = Button_Update(&g_button1);

    if (btn_event == BUTTON_EVENT_SHORT)
    {
      itm_print("BUTTON SHORT\r\n");
      App_HandleShortPress(&app);
    }
    else if (btn_event == BUTTON_EVENT_LONG)
    {
      itm_print("BUTTON LONG\r\n");
      App_HandleLongPress(&app);
    }

    if (app.measurement_running)
    {
      if (scd4x_runtime_poll(&app.runtime))
      {
        app.measurement_has_data = true;
      }

      app.sensor_connected = (app.runtime.error == NO_ERROR);
      if (!app.sensor_connected)
      {
        app.measurement_has_data = false;
      }
    }
    else if ((now_ms - app.last_link_check_ms) >= 500U)
    {
      app.last_link_check_ms = now_ms;
      app.sensor_connected = (Sensor_CheckConnection() == NO_ERROR);
    }

    App_UpdateLedMode(&app);
    LED_ApplyMode(&app, now_ms);

    osDelay(20);

  }
  /* USER CODE END 5 */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM4 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM4)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
