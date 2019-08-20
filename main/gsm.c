/*
 * Modulo gsm.c
 * 	Este modulo implementa el manejo del modulo GSM a traves del driver gsm.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "sdkconfig.h"
#include "gsmdriver.h"
#include "gsm.h"
#include "define.h"

#define DEBUG_GSM 0

enum GSMStatus{GSM_INIT, GSM_CONFIGURE, GSM_READY, GSM_STOPED, GSM_SMS_SEND};

static uint32_t	GSMStatusMachine;
static TSystemEvent FGSMSystemEvent;
static QueueHandle_t FEventQueueGSM;

/**
 * 	GSMTask:
 * 		Tarea periodica de periodo 200ms, detecta , inicializa y configura el modulo GSM
 * 		comunica el resultado a traves de una cola de mensajes.
 * */
static void GSMTask(void *pvParameters)
{
#if DEBUG_GSM
	printf("GSM INIT\r\n");
#endif
	GSMStatusMachine = GSM_INIT;
	const TickType_t GSMFrequency = 200 / portTICK_PERIOD_MS;
	TickType_t TimeTicks = xTaskGetTickCount();
	uint32_t Result;
	vTaskDelay(2000 / portTICK_PERIOD_MS);
	while (1) {
		switch(GSMStatusMachine){
		case 	GSM_INIT:																//	Etapa 1 inicializacion
			Result = GSMDriverStartProcess();
			if(Result == GSM_OK)
				GSMStatusMachine = GSM_CONFIGURE;
			else if(Result == GSM_TIMEOUT){
				GSMStatusMachine = GSM_STOPED;
				FGSMSystemEvent.EventID = GSM_DEVICE_NOT_DETECTED;
				FGSMSystemEvent.Data = 0;
				xQueueSend(FEventQueueGSM,(void *)&FGSMSystemEvent,portMAX_DELAY);		//	Avisamos resultado	de la inicializacion
			}
			break;
		case 	GSM_CONFIGURE:															//	Etapa 2 configuracion
			Result = GSMDriverConfigureProcess();
			if(Result == GSM_OK){
				GSMStatusMachine = GSM_READY;
				FGSMSystemEvent.EventID = GSM_DEVICE_INIT_OK;
				FGSMSystemEvent.Data = 0;
				xQueueSend(FEventQueueGSM,(void *)&FGSMSystemEvent,portMAX_DELAY);
			}
			else if(Result == GSM_TIMEOUT){
				GSMStatusMachine = GSM_STOPED;
				FGSMSystemEvent.EventID = GSM_DEVICE_CONFIGURE_FAIL;
				FGSMSystemEvent.Data = 0;
				xQueueSend(FEventQueueGSM,(void *)&FGSMSystemEvent,portMAX_DELAY);		//	Avisamos resultado de la configuracion
			}
			break;
		case 	GSM_READY:																//	Modulo inicializado y registrado en la red gsm
			break;
		case	GSM_STOPED:
			break;
		case	GSM_SMS_SEND:															//	envio de un sms
			Result =  GSMDriverSendSMS();
			if(Result == GSM_OK){
				GSMStatusMachine = GSM_STOPED;
				FGSMSystemEvent.EventID = GSM_DEVICE_SEND_SMS_OK;
				FGSMSystemEvent.Data = 0;
				xQueueSend(FEventQueueGSM,(void *)&FGSMSystemEvent,portMAX_DELAY);
			}
			else if(Result == GSM_TIMEOUT){
				GSMStatusMachine = GSM_STOPED;
				FGSMSystemEvent.EventID = GSM_DEVICE_SEND_SMS_FAIL;
				FGSMSystemEvent.Data = 0;
				xQueueSend(FEventQueueGSM,(void *)&FGSMSystemEvent,portMAX_DELAY);		//	Avisamos resultado del envio
			}
			break;
		default:
			GSMStatusMachine = GSM_INIT;
			break;
		}
		vTaskDelayUntil(&TimeTicks,GSMFrequency);
	}
}

/**
 *	SetSMStoSend:
 *		Establece el mensaje a enviar y activa el envio.
 *	Parametros:
 *		char *AMessage		puntero al mensaje
 *	Retorna:
 *		0	OK
 *		-1	Falla
 * */
int32_t SetSMStoSend(char *AMessage)
{
	if(GSMStatusMachine != GSM_SMS_SEND){
		GSMDriverSetMessage(AMessage);			//	Pasamos el mensaje al driver
		GSMStatusMachine = GSM_SMS_SEND;		//	Iniciamos la maquina para el envio
	}
	else
		return -1;
	return 0;
}
/*
 * 	GSMInit:
 * 		Inicializa el modulo.
 * 	Parametros:
 * 		QueueHandle_t AEventQueue	Valor del controlador de la cola de mensajes pasado por parametros
 * 		UBaseType_t APriority		Prioridad de la tarea que controla el modulo
 * */
void GSMInit(QueueHandle_t AEventQueue, UBaseType_t APriority)
{
	FEventQueueGSM = AEventQueue;															//	guardamos el valor del handler de la cola de mensajes
	GSMStatusMachine = GSM_INIT;
	GSMDriverInit();																		//	inicializamos el driver
	xTaskCreate(GSMTask, "GSMTask", configMINIMAL_STACK_SIZE + 2048, NULL, APriority, NULL);
}

