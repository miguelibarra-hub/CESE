/*
 *
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "sdkconfig.h"
#include "gsm.h"
#include "samd21.h"
#include "leds.h"
#include "define.h"

#define QUEUE_LENGTH	10									//	maxima cantidad de items en la cola de mensajes

/*** Definicion de las prioridades ****/
#define VERY_HIGH_PRIORITY	(configMAX_PRIORITIES - 0)
#define HIGH_PRIORITY		(configMAX_PRIORITIES - 1)
#define MEDIUM_PRIORITY		(configMAX_PRIORITIES - 2)
#define LOW_PRIORITY		(configMAX_PRIORITIES - 3)

static QueueHandle_t FEventQueue;
static TSystemEvent FSystemEvent;
static uint32_t FDeviceNumberOK;

#define DEBUG_MAIN	1

#if DEBUG_MAIN

#endif
/**
 * 	ControlTask:
 * 		Tarea no periodica, se queda esperando por siempre por los eventos de las demas tareas
 * 		realiza funciones segun el evento generado
 * */
static void ControlTask(void *pvParameters)
{
	while(1)
	{
		if(xQueueReceive(FEventQueue, &FSystemEvent, portMAX_DELAY) == pdTRUE){			//	Se queda aca hasta que recibe algun mensaje en la cola
			switch(FSystemEvent.EventID){												//	Vemos que mensaje llego
			case 	GSM_DEVICE_INIT_OK:													//	Modulo GSM inicializado, configurado y registrado
#if DEBUG_MAIN
				printf("GSM_DEVICE_INIT_OK\r\n");
#endif
				FDeviceNumberOK++;														//	Incremento contador de dispositivos OK
				SetLedMode(LED_ON,0,LED_LINK,0);										//	Encendemos led Link indicando registro en red GSM
				break;
			case	GSM_DEVICE_CONFIGURE_FAIL:											//	Fallo la configuracion
#if DEBUG_MAIN
				printf("GSM_DEVICE_CONFIGURE_FAIL\r\n");
#endif
				SetLedMode(LED_ON,0,LED_ACTIVITY,0);									//	Encendemos en forma permanente el led de Actividad indicando el error
				SetLedMode(LED_OFF,0,LED_LINK,0);										//	Apagamos el led Link
				break;
			case	GSM_DEVICE_NOT_DETECTED:											//	No se detecto el modulo GSM
#if DEBUG_MAIN
				printf("GSM_DEVICE_NOT_DETECTED\r\n");
#endif
				SetLedMode(LED_ON,0,LED_ACTIVITY,0);									//	Encendemos en forma permanente el led de Actividad indicando el error
				SetLedMode(LED_OFF,0,LED_LINK,0);										//	Apagamos el led Link
				break;
			case	SAM_DEVICE_NOT_DETECTED:											//	SAMD21 no detectado
#if DEBUG_MAIN
				printf("SAM_DEVICE_NOT_DETECTED\r\n");
#endif
				SetLedMode(LED_ON,0,LED_ACTIVITY,0);									//	Encendemos en forma permanente el led de Actividad indicando el error
				break;
			case	SAM_DEVICE_OK:														//	SAMD21	OK
#if DEBUG_MAIN
				printf("SAM_DEVICE_OK\r\n");
#endif
				FDeviceNumberOK++;														//	Incremento contador de dispositivos OK
				break;
			case	SAM_MESSAGE_READY:													//	Hay un mensaje para enviar por SMS
#if DEBUG_MAIN
				printf("SAM_MESSAGE_READY\r\n");
#endif
				SetSMStoSend((char *)FSystemEvent.Data);								//	Pasamos el puntero del mensaje al modulo gsm.c
				break;
			case	GSM_DEVICE_SEND_SMS_OK:												//	Mensaje enviado OK
#if DEBUG_MAIN
				printf("GSM_DEVICE_SEND_SMS_OK\r\n");
#endif
				SetLedMode(LED_BLINK,3,LED_LINK,5);										//	Realizamos 5 destellos por el led Link a 300ms indicando
				SAMD21FreeCommunicationChannel();										//	Liberamos el canal
				break;
			case	GSM_DEVICE_SEND_SMS_FAIL:											//	Fallo el envio del mensaje
#if DEBUG_MAIN
				printf("GSM_DEVICE_SEND_SMS_FAIL\r\n");
#endif
				SAMD21FreeCommunicationChannel();										//	Liberamos el canal
				break;
			default:
				break;
			}
			if(FDeviceNumberOK == 2)													//	Chequeamos la cantidad de dispositivos Ok
				SetLedMode(LED_BLINK,10,LED_ACTIVITY,0);								//	Todos OK cambiamos la frecuencia del led de actividad a 1Hz
		}else{
			printf("Item recibido en forma incorrecta !!!!\r\n");
		}

	}
}

void app_main()
{
	FDeviceNumberOK = 0;
	FEventQueue = xQueueCreate(QUEUE_LENGTH, sizeof(TSystemEvent));						//	Crea la cola de mensajes
	if(FEventQueue != 0){																//	Verifica el handler de la cola
		xTaskCreate(ControlTask, "ControlTask", configMINIMAL_STACK_SIZE + 2048, NULL, HIGH_PRIORITY, NULL);	//	Asignamos la mayor prioridad a la tarea no periodica
		SAMD21Init(FEventQueue, MEDIUM_PRIORITY);
		GSMInit(FEventQueue, MEDIUM_PRIORITY);
		LedsInit(LOW_PRIORITY);															//	Asignamos baja prioridad por ser solo señalizacion
		SetLedMode(LED_BLINK, PERIODO_500_MS, LED_ACTIVITY,0);							//	inicio con 500ms de parpadeo inidica inicializacion en progreso
		SetLedMode(LED_BLINK, PERIODO_500_MS, LED_LINK,0);								//	inicio con 500ms de parpadeo indica buscando red gsm
#if DEBUG_MAIN
		printf("SYSTEM INIT\n");
#endif
	}else{
		printf("SYSTEM FAILURE - Create Queue Fail\n");
	}
	while(1){};
}
