/**
 * Modulo leds.c
 * 	Este modulo se encarga del control de dos leds, Actividad y Enlace/Datos
 *
 *
 **/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "leds.h"

/***** Numero de GPIO de la placa *****/
#define ACTIVITY_GPIO 	13
#define LINK_GPIO		12

SemaphoreHandle_t	LedsSemaphore;		//	Mutex usado para acceder al recurso compartido FLedArray[]

/***** Estructura de control de cada led ****/
typedef struct
{
	uint32_t	GpioPin;				//	Pin donde se conecta el led
	uint32_t	LedStatus;				//	Estado del led , define el comportamiento del mismo
	uint32_t	Period;					//	Periodo de la pulsacion
	uint32_t	InitLevel;				//	Nivel inicial
	uint32_t 	Timeout;				//	Variable usada para controlar la duracion del pulso
	uint32_t	BlinkyCount;			//	Numero de destellos
	uint32_t	LedStatusOld;			//	Variable usada para para almacenar el estado actual del led cuando debe realizar destellos fijos
}TLedConfig;

static TLedConfig FLedArray[MAX_LEDS_LENGTH];	//	Estructura principal donde se almacenan los leds

/*
 * 	LedMachineState:
 * 		Maquina de estados que controla el comportamiento de los leds es llamada periodicamente cada 100ms
 * */
static void LedMachineState(uint32_t Canal)
{
	switch(FLedArray[Canal].LedStatus){
		case	LED_BLINK:														//	Estado Blink el led parpadea
			if(FLedArray[Canal].Timeout != 0){
				FLedArray[Canal].Timeout--;										//	Descuenta el timeout de duracion de pulso
				if(FLedArray[Canal].Timeout == 0){
					gpio_set_level(FLedArray[Canal].GpioPin, 1);
					if(FLedArray[Canal].InitLevel == 1){						//	Si expiro el timeout cambiamos el nivel del led segun InitLevel
						gpio_set_level(FLedArray[Canal].GpioPin, 1);			//	Activa led en 1
						FLedArray[Canal].InitLevel = 0;
					}
					else{
						gpio_set_level(FLedArray[Canal].GpioPin, 0);			//	Activa led en 0
						FLedArray[Canal].InitLevel = 1;
					}
					FLedArray[Canal].Timeout = FLedArray[Canal].Period;			//	Chequea si hay que controlar la cantidad de destellos
					if(FLedArray[Canal].BlinkyCount != 0){
						FLedArray[Canal].BlinkyCount--;
						if(FLedArray[Canal].BlinkyCount == 0){					//	Si no hay mas destellos que realizar vuelve al estado anterior
							FLedArray[Canal].LedStatus = FLedArray[Canal].LedStatusOld;
						}
					}
				}
			}
			break;
		case	LED_ON:														//	Estado ON siempre prendido
			gpio_set_level(FLedArray[Canal].GpioPin, 1);
			break;
		case	LED_OFF:													//	Estado OFF siempre apagado
			gpio_set_level(FLedArray[Canal].GpioPin, 0);
			break;
		default:
			FLedArray[Canal].LedStatus = LED_OFF;							//	Por defecto apagado
			break;
		}
}
/**
 * 	LedsTask:
 * 		Tarea periodica de periodo 100ms. Llama a maquina de estados que controla la temporizacion y estado de los leds.
 * */
static void LedsTask(void *pvParameters)
{
	TickType_t TimeTicks = xTaskGetTickCount();
	while(1) {
		if(xSemaphoreTake(LedsSemaphore,10 / portTICK_PERIOD_MS )){			//	Intentamos obtener el semaforo espera un tiempo si esta ocupado
			LedMachineState(LED_ACTIVITY);
			LedMachineState(LED_LINK);
			xSemaphoreGive(LedsSemaphore);									//	Una ves que terminamos de procesar lo liberamos
		}
		vTaskDelayUntil(&TimeTicks,100 / portTICK_PERIOD_MS);
    }
}

/**
 * 	SetLedMode:
 * 		Funcion usada para configurar el comportamiento de cada led.
 * 	Parametros:
 * 		uint32_t AMode				Modo de funcionamiento
 * 		uint32_t APeriodo			Periodo del led
 * 		uint32_t AChannelLed		Canal seleccionado
 * 		uint32_t ABlinkyCount 		Cantidad de destellos
 *
 * */
void SetLedMode(uint32_t AMode, uint32_t APeriodo, uint32_t AChannelLed, uint32_t ABlinkyCount)
{
	if(xSemaphoreTake(LedsSemaphore,portMAX_DELAY)){									//	Intenta acceder al recurso compartido, se queda esperando hasta
		if((ABlinkyCount != 0))															//	que se libera para poder informa al usuario el evento de led
			FLedArray[AChannelLed].LedStatusOld = FLedArray[AChannelLed].LedStatus;
		FLedArray[AChannelLed].LedStatus = AMode;
		FLedArray[AChannelLed].Period = APeriodo;
		FLedArray[AChannelLed].Timeout = APeriodo;
		FLedArray[AChannelLed].BlinkyCount += (ABlinkyCount * 2);
		xSemaphoreGive(LedsSemaphore);													//	Libera el recurso compartido.
	}
}

/**
 * 	LedsInit:
 * 		Inicializa el modulo.
 *	Parametros
 *		UBaseType_t APriority		Prioridad de la tarea que controla el modulo
 * */
void LedsInit(UBaseType_t APriority)
{
	gpio_pad_select_gpio(ACTIVITY_GPIO);					//	Configura el pin ACTIVITY_GPIO como GPIO
	gpio_set_direction(ACTIVITY_GPIO, GPIO_MODE_OUTPUT);  	//	Configura el pin ACTIVITY_GPIO como salida
	gpio_pad_select_gpio(LINK_GPIO);						//	Configura el pin LINK_GPIO como GPIO
	gpio_set_direction(LINK_GPIO, GPIO_MODE_OUTPUT);	 	//	Configura el pin LINK_GPIO como salida
	gpio_set_level(ACTIVITY_GPIO, 0);
	gpio_set_level(LINK_GPIO, 0);
	/***** LED de Actividad ******/
	FLedArray[0].GpioPin = ACTIVITY_GPIO;
	FLedArray[0].LedStatus = LED_OFF;						//	Inicialmente apagado
	FLedArray[0].Period = 5;
	FLedArray[0].InitLevel = 1;
	FLedArray[0].Timeout = 5;
	FLedArray[0].BlinkyCount = 0;
	FLedArray[0].LedStatusOld = LED_OFF;
	/***** LED de Link ******/
	FLedArray[1].GpioPin = LINK_GPIO;
	FLedArray[1].LedStatus = LED_OFF;						//	Inicialmente apagado
	FLedArray[1].Period = 2;
	FLedArray[1].InitLevel = 1;
	FLedArray[1].Timeout = 2;
	FLedArray[1].BlinkyCount = 0;
	FLedArray[1].LedStatusOld = LED_OFF;
	LedsSemaphore = xSemaphoreCreateMutex();
	xTaskCreate(LedsTask, "LedsTaskActivity", configMINIMAL_STACK_SIZE, 0, APriority, NULL);
}
