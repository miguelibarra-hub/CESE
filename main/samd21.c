/*
 * Modulo samd21.c
 * 		Este modulo implementa la interface serie con el microcontrolador SAMD21.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "sdkconfig.h"
#include "samd21.h"
#include "define.h"

/*   Protocolo de comunicacion con el micro SAMD21
 *	 Master ESP32            SLAVE SAMD21
 *
 *	     0xE0       ---->									-	Master interroga al esclavo y avisa que esta listo para recibir datos
 *	     		    <----		0xE0     					- 	Esclavo presente sin datos para transmitir
 *
 *	     0xE0       ---->
 *	     		    <----		Paquete de Datos			-	Esclavo envia datos al master
 *
 *	     0xE1		---->									-	Master ocupado procesando un paquete anterior.
 *	     			<----		0xE0 o 0xE2					-	El esclavo solo transmite su estado E0 = Sin Datos E2 = con datos pendientes
 *
 *	  Formato del paquete
 *   ------------------------------
 *   | Datos en formato de texto |
 *	 ------------------------------
 *	 no se incluye CRC por estar los dos micros en la misma placa.
 * */
#define SAMD21_TXD1  (GPIO_NUM_4)
#define SAMD21_RXD1  (GPIO_NUM_5)
#define SAMD21_RTS1  (UART_PIN_NO_CHANGE)
#define SAMD21_CTS1  (UART_PIN_NO_CHANGE)

#define BUF_SIZE_SAMD21 (1024)
#define BUF_SIZE_SAM	40
#define MAX_RETRY_INIT	10		//	Maxima cantidad de reintentos de comunicacion con el SAMD21

#define SCAN_COMMAND	0xE0
#define BUSY_COMMAND	0xE1

#define DEBUG_SAMD21	0

enum SAMStatus{SAM_INIT, SAM_WAIT_SLAVE_ANSWER, SAM_IDLE, SAM_ERROR};

static uint8_t 	BufferSAM[BUF_SIZE_SAM];
static char 	BufferMessage[BUF_SIZE_SAM];
static uint32_t	FSAMStatusMachine;
static uint32_t FCommunicationLinkStatus;
static TSystemEvent FSAMSystemEvent;
static QueueHandle_t FEventQueueSAM;
static uint32_t	FSAMRetryTimeOut;
static uint32_t FFlowDirection;
static uint32_t	FSAMBusy;

/**
 * 	SAMD21FreeCommunicationChannel:
 * 		Libera el canal de comunicaciones con el SAMD21 para un nuevo mensaje
 * */
void SAMD21FreeCommunicationChannel(void)
{
	FSAMBusy = false;
}

/*
 * 	SAMD21Uartinit:
 * 		Inicializa la UART1 que se usa de interface con el micro SAMD21
 * */
static void SAMD21Uartinit(void)
{
	/*** Configuracion de la UART ***/
	uart_config_t uart_config = {
			.baud_rate = 115200,
			.data_bits = UART_DATA_8_BITS,
			.parity    = UART_PARITY_DISABLE,
			.stop_bits = UART_STOP_BITS_1,
			.flow_ctrl = UART_HW_FLOWCTRL_DISABLE
	};
	uart_param_config(UART_NUM_1, &uart_config);												//	Aplica la configuracion a la UART1
	uart_set_pin(UART_NUM_1, SAMD21_TXD1, SAMD21_RXD1, SAMD21_RTS1, SAMD21_CTS1);				//	Selecciona los pines a usar por TX y RX
	uart_driver_install(UART_NUM_1, BUF_SIZE_SAMD21 * 2, 0, 0, NULL, 0);						//	Instala el driver
}
/*
 * 	CheckResponseFromSAM:
 *		Verifica si recibio datos desde el SAMD21
 *	Parametros:
 *		uint32_t *ALength				Puntero donde se almacenara la cantidad de datos que llegaron
 *		uint8_t *ABufferResponse		Puntero donde se almacenaran los datos
 *	Retorna:
 *		true	hay datos
 *		false	no llego nada
 * */
static uint32_t CheckResponseFromSAM(uint32_t *ALength, uint8_t *ABufferResponse)
{
	uint32_t Lenght = 0;
	if(uart_get_buffered_data_len(UART_NUM_1, (size_t*)&Lenght) == ESP_OK){		//	Chequeamos si recibimos algo
		*ALength = uart_read_bytes(UART_NUM_1, ABufferResponse, Lenght, 0);		//	Guardamos lo recibido y actualizamos la cantidad
		return true;
	}else
		return false;
}

/*
 * 	SAMD21Task:
 * 		Tarea periodica de periodo 100ms que controla las comunicaciones con el micro SAMD21
 * 		Determina si esta presente y recibe los mensajes a enviar por sms.
 * 		Los mismos son enviados a la cola de mensajes para que lo procese otra tarea.
 * */
static void SAMD21Task(void *pvParameters)
{
	uint32_t Length = 0;
	uint32_t i = 0;
	FSAMRetryTimeOut = MAX_RETRY_INIT;
	const TickType_t SAMFrequency = 100 / portTICK_PERIOD_MS;
	TickType_t TimeTicks = xTaskGetTickCount();
	while (1) {
		char DataSend = 0;
		switch(FSAMStatusMachine){
		case	SAM_INIT:
			DataSend = SCAN_COMMAND;
			uart_write_bytes(UART_NUM_1, &DataSend, sizeof(DataSend));						//	Enviamos el byte de exploracion
			FSAMStatusMachine = SAM_WAIT_SLAVE_ANSWER;
			break;
		case	SAM_WAIT_SLAVE_ANSWER:														//	Esperamos que el SAMD21 responda
			if(CheckResponseFromSAM(&Length, &BufferSAM[0])){
				if((Length == 1) && (BufferSAM[0] == SCAN_COMMAND)){
					FCommunicationLinkStatus = true;
					FSAMSystemEvent.EventID = SAM_DEVICE_OK;								//	Si responde comunicamos el evento
					FSAMSystemEvent.Data = 0;
					xQueueSend(FEventQueueSAM,(void *)&FSAMSystemEvent,portMAX_DELAY);		//	Avisamos que el SAMD21 esta OK
					FSAMStatusMachine = SAM_IDLE;
					FFlowDirection = true;
				}
			}
			if(FSAMStatusMachine == SAM_WAIT_SLAVE_ANSWER){
				if(FSAMRetryTimeOut != 0){
					FSAMRetryTimeOut--;														//	Actualizamos un timeout de respuesta
					if(FSAMRetryTimeOut == 0){
						FCommunicationLinkStatus = false;
						FSAMSystemEvent.EventID = SAM_DEVICE_NOT_DETECTED;					//	Si expira el timeout avisamos que no se detecto
						FSAMSystemEvent.Data = 0;
						xQueueSend(FEventQueueSAM,(void *)&FSAMSystemEvent,portMAX_DELAY);	//	Avisamos que el SAMD21 no contesta
						FSAMStatusMachine = SAM_ERROR;
					}
				}
			}
			break;
		case	SAM_IDLE:																	//	Si el SAMD21 esta OK
			if(FFlowDirection){																//	Determina si hay que enviar o ver si se recibio algo
				if(FSAMBusy)																//	Verifica si el modulo esta ocupado con algun mensaje previo
					DataSend = BUSY_COMMAND;												//	Modulo ocupado envia BUSY_COMMAND
				else
					DataSend = SCAN_COMMAND;												//	Modulo desocupado envia SCAN_COMMAND
				uart_write_bytes(UART_NUM_1, &DataSend, sizeof(DataSend));					//	Enviamos el byte de exploracion
				FFlowDirection = false;
			}else{
				if(CheckResponseFromSAM(&Length, &BufferSAM[0])){							//	Chequeamos respuesta
					if((Length > 1) && (Length < BUF_SIZE_SAM)){
						for(i = 0; i < (Length -1);i++){
							BufferMessage[i] = BufferSAM[i+1];								//	cargamos los datos recibidos
						}
						BufferMessage[i] = 0;												//	finalizamos en cero por las dudas
						FSAMBusy = true;													//	ocupamos el modulo
#if DEBUG_SAMD21
						printf("lenght = %d \r\n",Length);
#endif
						FSAMSystemEvent.EventID = SAM_MESSAGE_READY;						//	Mensaje listo
						FSAMSystemEvent.Data = (void *)&BufferMessage[0];					//	guardamos el puntero al mensaje
						xQueueSend(FEventQueueSAM,(void *)&FSAMSystemEvent,portMAX_DELAY);	//	Avisamos que tenemos un mensaje listo
					}
				}
				FFlowDirection = true;
			}
			break;
		case	SAM_ERROR:																	//	Estado de la muerte
			break;
		default:
			break;
		}
		vTaskDelayUntil(&TimeTicks,SAMFrequency);
	}

}
/**
 * 	SAMD21Init:
 * 		Inicializa el modulo
 * 	Parametros:
 * 		QueueHandle_t AEventQueue			Recibe como parametro el valor de la cola de mensajes
 * 		UBaseType_t APriority		Prioridad de la tarea que controla el modulo
 * */
void SAMD21Init(QueueHandle_t AEventQueue, UBaseType_t APriority)
{
	FEventQueueSAM = AEventQueue;
	SAMD21Uartinit();
	FSAMBusy = false;
	FFlowDirection = true;
	FSAMStatusMachine = SAM_INIT;
	FCommunicationLinkStatus = false;
	xTaskCreate(SAMD21Task, "uart1_echo_task", configMINIMAL_STACK_SIZE + 2048, NULL, APriority, NULL);
}
