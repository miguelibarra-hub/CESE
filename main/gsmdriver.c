/*
 * Modulo gsmdriver.c
 *	Este modulo implementa un driver para el control del modulo GSM a traves de la UART.
 *	Controla la deteccion, inicializacion , configuracion y el envio de mensajes.
 *
 */

#ifndef MAIN_GSMDRIVER_C_
#define MAIN_GSMDRIVER_C_

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "gsmdriver.h"
#include "define.h"
#include "sdkconfig.h"

#define DEBUG_GSM_DRIVER	1			//	Habilita / Deshabilita los mensajes de debug del modulo

#define GSM_TXD2  (GPIO_NUM_17)
#define GSM_RXD2  (GPIO_NUM_16)
#define GSM_RTS2  (UART_PIN_NO_CHANGE)
#define GSM_CTS2  (UART_PIN_NO_CHANGE)

#define MAX_RETRY_SYNCRO		10		//	Maxima cantidad de reintentos de sincronizacion
#define TIME_BETWEEN_ATTEMPT	10		//	Tiempo entre intentos de sincronismo
#define TIME_FOR_WAIT_SMS_SEND	100		//	Tiempo para esperar el OK del envio de SMS
#define BUF_SIZE_GSM_UART		1024		//	Tamaño del buffer de recepcion

#define ESC_CHAR				0x1A	//	Caracter de escape 	que se agrega al final de un texto SMS para indicar que alli finaliza

/****** Estados de las maquinas de estados que controlan el modulo GSM ******/
enum GSMDriverStatus{GSM_START_SYNCRO, GSM_WAIT_SYNCRO, GSM_SEND_AT,GSM_WAIT_OK,
					 GSM_CONFIGURE_STEP0, GSM_WAIT_STEPO_RESULT,
					 GSM_CONFIGURE_STEP1, GSM_WAIT_STEP1_RESULT,
					 GSM_CONFIGURE_STEP2,
					 GSM_SEND_SMS_STEP0, GSM_SEND_SMS_WAIT_STEP0_RESULT,
					 GSM_SEND_SMS_STEP1, GSM_SEND_SMS_WAIT_STEP1_RESULT,
					 GSM_SEND_SMS_STEP2, GSM_SEND_SMS_WAIT_STEP2_RESULT,
					 GSM_SEND_SMS_FAIL, GSM_SEND_SMS_END};

static uint32_t FGSMProcessStatus;
static uint32_t FGSMProcessTimeOut;
static uint32_t	FRetryTimeOut;
static uint8_t DataBufferRx[BUF_SIZE_GSM_UART];			//	Buffer de recepcion
static char *FMessage;
static char* CelphoneNumber = CELPHONE_NUMBER;
/**
 * 	SearchStringInBuffer:
 * 		Busca una cadena de tecto en un buffer dado.
 * 	Parametros:
 * 		const char* AStringToSearch		puntero a la cadena de texto
 * 		uint8_t *ABuffer				puntero al buffer donde se busca la cadena
 * 	Retorna:
 * 		1		cadena encontrada
 * 		0		cadena no encontrada
 * */
static uint32_t SearchStringInBuffer(const char* AStringToSearch, uint8_t *ABuffer)
{
	if(strstr((const char*)ABuffer,AStringToSearch) != 0)
		return true;
	else
		return false;
}
/**
 * 	BufferClear:
 * 		Inicializa un buffer con cero.
 * 	Parametros:
 * 		uint8_t *ABuffer		Puntero al buffer a inicializar
 * 		uint32_t ALenght	 	Tamaño del buffer
 * */
static void BufferClear(uint8_t *ABuffer, uint32_t ALenght)
{
	for(uint32_t i = 0; i < ALenght; i++)
		*(ABuffer + i) = 0;
}
/**
 * 	GSMDriverInit:
 * 		Inicializa el modulo
 * */
void GSMDriverInit(void)
{
	/*** Configuracion de la UART ***/
	uart_config_t uart_config = {
			.baud_rate = 9600,
			.data_bits = UART_DATA_8_BITS,
			.parity    = UART_PARITY_DISABLE,
			.stop_bits = UART_STOP_BITS_1,
			.flow_ctrl = UART_HW_FLOWCTRL_DISABLE
	};
	uart_param_config(UART_NUM_2, &uart_config);												//	Aplica la configuracion a la UART2
	uart_set_pin(UART_NUM_2, GSM_TXD2, GSM_RXD2, GSM_RTS2, GSM_CTS2);	//	Selecciona los pines a usar por TX y RX
	uart_driver_install(UART_NUM_2, (1024) * 2, 0, 0, NULL, 0);									//	Instala el driver
	FGSMProcessStatus = GSM_START_SYNCRO;														//	Estado inicial de la  maquina de estados
	FRetryTimeOut = MAX_RETRY_SYNCRO;															//	Configura la cantidad maxima de reintentos de comunicacion
	FMessage = 0;
}

/**
 * 	CheckResponseFromModule:
 * 		Verifica si llegaron datos desde el modulo GSM. Y busca una respuesta especifica en el buffer de recepcion.
 * 	Parametros:
 * 		const char *AResponse				cadena de texto a buscar
 *	Retorna:
 *		1		cadena encontrada
 *		0		cadena no encontrada o no hubo respuesta todavia.
 * */
static uint32_t CheckResponseFromModule(const char *AResponse)
{
	uint32_t Lenght = 0;
	BufferClear(&DataBufferRx[0],BUF_SIZE_GSM_UART);
	Lenght = uart_read_bytes(UART_NUM_2, &DataBufferRx[0], BUF_SIZE_GSM_UART, 20 / portTICK_PERIOD_MS);
	if(SearchStringInBuffer(AResponse,&DataBufferRx[0])){
		return true;
	}else
		return false;
	if(Lenght == 0)
		return false;

}

/**
 * 	CheckTimeOutProcess:
 * 		Actualiza el timeout de espera de respuesta del modulo.
 * 	Retorna:
 * 		1		timeout
 * 		0		sin timeout
 * */
static uint32_t CheckTimeOutProcess(void)
{
	if(FGSMProcessTimeOut != 0){
		FGSMProcessTimeOut--;
		if(FGSMProcessTimeOut == 0){
			return true;
		}else
			return false;
	}return true;
}

/**
 * 	GSMDriverStartProcess:
 * 		Maquina de estados que realiza el proceso de deteccion y sincronizacion inicial del modulo GSM.
 * 		Esta funcion es llamada desde un modulo de mayor nivel y retorna cada ves que es llamada el estado
 * 		del proceso que controla.
 * 		Descripcion de los Estados.
 * 		GSM_START_SYNCRO --->  	envia el comando "AT" hacia el modulo GSM.
 * 		GSM_WAIT_SYNCRO  --->  	espera que se agote el timeout de espera de respuesta para verifica si el modulo respondio
 * 								si no responde o no es la respuesta esperada decrementa la cantidad de reintentos y vuelve al estado inicial
 * 								si se agotan los reintentos avisa Timeout.
 * 	Retorna:
 * 		GSM_IN_PROGRESS			Proceso de Inicio en progreso
 * 		GSM_TIMEOUT				El modulo no responde
 * 		GSM_OK					Proceso de Inicio Exitoso
 * */
uint32_t GSMDriverStartProcess(void)
{
	uint32_t Result = GSM_IN_PROGRESS;
	switch(FGSMProcessStatus){
	case	GSM_START_SYNCRO:											//	Envia la cadena de sincronizacion
		uart_write_bytes(UART_NUM_2, "AT\r", sizeof("AT\r")-1);			//	Escribe a la UART2
		FGSMProcessStatus = GSM_WAIT_SYNCRO;							//	Cambiamos de estado para esperar la respuesta
		FGSMProcessTimeOut = TIME_BETWEEN_ATTEMPT;						//	Inicializamos el timeout de espera de respuesta
		Result = GSM_IN_PROGRESS;										//	Proceso en progreso
		break;
	case	GSM_WAIT_SYNCRO:											//	Espera respuesta
		if(CheckTimeOutProcess()){										//	Verifica Timeout
			if(CheckResponseFromModule("OK")){							//	Timeout Verifica si recibio la cadena "OK"
				FGSMProcessStatus = GSM_CONFIGURE_STEP0;				//	Proximo estado si el modulo responde correctamente
				FRetryTimeOut = MAX_RETRY_SYNCRO;
				Result = GSM_OK;										//	Proceso de inicio OK
#if DEBUG_GSM_DRIVER
				printf("Driver - SYNCRO OK\r\n");
#endif
			}else{
				FGSMProcessStatus = GSM_START_SYNCRO;					//	Sin respuesta, volvemos a intentar
				FRetryTimeOut--;										//	Actualizamos reintentos
				if(FRetryTimeOut == 0){
					Result = GSM_TIMEOUT;								//	Se vencio la cantidad de reintentos sin respuesta
#if DEBUG_GSM_DRIVER
					printf("Driver - GSM_TIMEOUT\r\n");
#endif
					FRetryTimeOut = MAX_RETRY_SYNCRO;
				}
			}
		}else															//	No se produjo timeout
			Result = GSM_IN_PROGRESS;
		break;
	default:
		FGSMProcessStatus = GSM_START_SYNCRO;
		Result = GSM_IN_PROGRESS;
		break;
	}
	return Result;
}

/**
 * GSMDriverConfigureProcess:
 * 		Maquina de estado que controla la configuracion del modulo GSM.
 * 		Esta funcion es llamada desde un modulo de mayor nivel y retorna cada ves que es llamada el estado
 * 		del proceso que controla.
 * 		Descripcion de los Estados.
 * 			GSM_CONFIGURE_STEP0		----->	Envia el comando ATE0 para eliminar el eco
 * 			GSM_WAIT_STEPO_RESULT	----->	Espera el resultado con timeout
 * 			GSM_CONFIGURE_STEP1		----->	Verifica si el modulo esta registrado en la red GSM
 * 			GSM_WAIT_STEP1_RESULT	----->	Espera el resultado con timeout
 *		Retorna:
 * 		GSM_IN_PROGRESS			Proceso en progreso
 * 		GSM_TIMEOUT				El modulo no responde
 * 		GSM_OK					Proceso Exitoso
 * */
uint32_t GSMDriverConfigureProcess(void)
{
	uint32_t Result = GSM_IN_PROGRESS;
	switch(FGSMProcessStatus){
	case	GSM_CONFIGURE_STEP0:												//	Envia el comando ATE0
		uart_write_bytes(UART_NUM_2, "ATE0\r", sizeof("ATE0\r"));				//	Escribe ala UART2
		FGSMProcessStatus = GSM_WAIT_STEPO_RESULT;								//	Cambiamos de estado para esperar la respuesta
		FGSMProcessTimeOut = TIME_BETWEEN_ATTEMPT;								//	Inicializamos el timeout de espera de respuesta
		Result = GSM_IN_PROGRESS;												//	Proceso en progreso
		break;
	case	GSM_WAIT_STEPO_RESULT:												//	Espera respuesta
		if(CheckTimeOutProcess()){												//	Verifica Timeout
			if(CheckResponseFromModule("OK")){									//	Timeout Verifica si recibio la cadena "OK"
				FGSMProcessStatus = GSM_CONFIGURE_STEP1;						//	Proximo estado si el modulo responde correctamente
				FRetryTimeOut = MAX_RETRY_SYNCRO;
				Result = GSM_IN_PROGRESS;										//	Proceso en progreso
#if DEBUG_GSM_DRIVER
				printf("Driver - GSM ATE0 OK\r\n");
#endif
			}else{
				FGSMProcessStatus = GSM_CONFIGURE_STEP0;						//	Sin respuesta, volvemos a intentar
				FRetryTimeOut--;												//	Actualizamos reintentos
				if(FRetryTimeOut == 0){
					Result = GSM_TIMEOUT;										//	Se vencio la cantidad de reintentos sin respuesta
					FRetryTimeOut = MAX_RETRY_SYNCRO;
#if DEBUG_GSM_DRIVER
					printf("Driver - GSM ATE0 TIMEOUT\r\n");
#endif
				}
			}
		}else
			Result = GSM_IN_PROGRESS;
		break;
	case	GSM_CONFIGURE_STEP1:												//	Verifica si se registro en la red GSM
		uart_write_bytes(UART_NUM_2, "AT+CREG?\r", sizeof("AT+CREG?\r"));		//	Escribe ala UART2
		FGSMProcessStatus = GSM_WAIT_STEP1_RESULT;
		FGSMProcessTimeOut = TIME_BETWEEN_ATTEMPT;
		Result = GSM_IN_PROGRESS;
		break;
	case	GSM_WAIT_STEP1_RESULT:												//	Espera respuesta
		if(CheckTimeOutProcess()){
			if(CheckResponseFromModule("+CREG: 1,1")){							//	Timeout Verifica si recibio la cadena "+CREG: 1,1"
				FGSMProcessStatus = GSM_SEND_SMS_STEP0;
				FRetryTimeOut = MAX_RETRY_SYNCRO;
				Result = GSM_OK;												//	Proceso OK
#if DEBUG_GSM_DRIVER
				printf("Driver - GSM +CREG OK\r\n");
#endif
			}else{
				FGSMProcessStatus = GSM_CONFIGURE_STEP1;						//	Sin respuesta, volvemos a intentar
				FRetryTimeOut--;
				if(FRetryTimeOut == 0){
					Result = GSM_TIMEOUT;										//	Se vencio la cantidad de reintentos sin respuesta
					FRetryTimeOut = MAX_RETRY_SYNCRO;
#if DEBUG_GSM_DRIVER
					printf("Driver - GSM +CREG TIMEOUT\r\n");
#endif
				}
			}
		}else
			Result = GSM_IN_PROGRESS;
		break;
	default:
		break;
	}
	return Result;
}

/**
 * 	GSMDriverSendSMS:
 *		Maquina de estados que controla el envio de SMS a un numero predeterminado.
 *		Esta funcion es llamada desde un modulo de mayor nivel y retorna cada ves que es llamada el estado
 * 		del proceso que controla.
 * 		Descripcion de los Estados.
 * 			GSM_SEND_SMS_STEP0				---->	Configura el modo AT+CMGF=1
 * 			GSM_SEND_SMS_WAIT_STEP0_RESULT	---->	Espera la respuesta
 * 			GSM_SEND_SMS_STEP1				---->	Configura el numero de celular al que va a enviar
 * 			GSM_SEND_SMS_WAIT_STEP1_RESULT	---->	Espera la respuesta
 * 			GSM_SEND_SMS_STEP2				---->	Escribe el mensaje de texto a enviar
 * 			GSM_SEND_SMS_WAIT_STEP2_RESULT	---->	Espera el resultado
 * 	Retorna:
 * 		GSM_IN_PROGRESS			Proceso en progreso
 * 		GSM_TIMEOUT				El modulo no responde
 * 		GSM_OK					Proceso Exitoso
 * */
uint32_t GSMDriverSendSMS(void)
{
	uint32_t Result = GSM_IN_PROGRESS;
	char Escape = ESC_CHAR;
	switch(FGSMProcessStatus){
	case	GSM_SEND_SMS_STEP0:
		uart_write_bytes(UART_NUM_2, "AT+CMGF=1\r", sizeof("AT+CMGF=1\r"));		//	Configura el modo SMS escribe por UART2
		FGSMProcessStatus = GSM_SEND_SMS_WAIT_STEP0_RESULT;
		FGSMProcessTimeOut = TIME_BETWEEN_ATTEMPT;								//	Inicializamos el timeout de espera de respuesta
		Result = GSM_IN_PROGRESS;
		break;
	case	GSM_SEND_SMS_WAIT_STEP0_RESULT:										//	Espera respuesta
		if(CheckTimeOutProcess()){
			if(CheckResponseFromModule("OK")){									//	Timeout Verifica si recibio la cadena "OK"
				FGSMProcessStatus = GSM_SEND_SMS_STEP1;							//	Proximo estado si el modulo responde correctamente
				FRetryTimeOut = MAX_RETRY_SYNCRO;
				Result = GSM_IN_PROGRESS;
#if DEBUG_GSM_DRIVER
				printf("Driver - AT+CMGF=1 OK\r\n");
#endif
			}else{
				FGSMProcessStatus = GSM_SEND_SMS_STEP0;							//	Sin respuesta, volvemos a intentar
				FRetryTimeOut--;
				if(FRetryTimeOut == 0){
					Result = GSM_TIMEOUT;										//	Se vencio la cantidad de reintentos sin respuesta
					FRetryTimeOut = MAX_RETRY_SYNCRO;
#if DEBUG_GSM_DRIVER
					printf("Driver - AT+CMGF=1 TIMEOUT\r\n");
#endif
				}
			}
		}else
			Result = GSM_IN_PROGRESS;
		break;
	case	GSM_SEND_SMS_STEP1:													//	Configura el numero de celular a enviar
		uart_write_bytes(UART_NUM_2, "AT+CMGS=\"+543513024449\"\r", sizeof("AT+CMGS=\"+543513024449\"\r"));
		//uart_write_bytes(UART_NUM_2, "AT+CMGS=" , sizeof("AT+CMGS="));
		//uart_write_bytes(UART_NUM_2, CelphoneNumber, strlen(CelphoneNumber));
		FGSMProcessStatus = GSM_SEND_SMS_WAIT_STEP1_RESULT;
		FGSMProcessTimeOut = TIME_BETWEEN_ATTEMPT;
		Result = GSM_IN_PROGRESS;
		FRetryTimeOut = 100;
		break;
	case	GSM_SEND_SMS_WAIT_STEP1_RESULT:										//	espera recibir ">" para escribir el mensaje
		if(CheckTimeOutProcess()){
			if(CheckResponseFromModule(">")){
				FGSMProcessStatus = GSM_SEND_SMS_STEP2;							//	vamos a escribir el mensaje
				FRetryTimeOut = MAX_RETRY_SYNCRO;
				Result = GSM_IN_PROGRESS;
#if DEBUG_GSM_DRIVER
				printf("Driver - AT+CMGS OK\r\n");
#endif
			}else{
				FRetryTimeOut--;
				if(FRetryTimeOut == 0){											//	Controlamos el timeout de respuesta
					Result = GSM_TIMEOUT;
					FRetryTimeOut = MAX_RETRY_SYNCRO;
					FGSMProcessStatus = GSM_SEND_SMS_FAIL;
#if DEBUG_GSM_DRIVER
					printf("Driver - AT+CMGS TIMEOUT\r\n");
#endif
				}
			}
		}else
			Result = GSM_IN_PROGRESS;
		break;
	case	GSM_SEND_SMS_STEP2:													//	escribimos el mensaje previamente cargado
		uart_write_bytes(UART_NUM_2, FMessage, strlen(FMessage));
		uart_write_bytes(UART_NUM_2, &Escape, sizeof(Escape));					//	Caracter de escape que indica fin de mensaje
		FGSMProcessStatus = GSM_SEND_SMS_WAIT_STEP2_RESULT;
		FGSMProcessTimeOut = TIME_FOR_WAIT_SMS_SEND;
		Result = GSM_IN_PROGRESS;
		break;
	case	GSM_SEND_SMS_WAIT_STEP2_RESULT:										//	Esperamos el resultado del sms enviado
		if(CheckTimeOutProcess()){
			if(CheckResponseFromModule("OK")){
				FGSMProcessStatus = GSM_SEND_SMS_END;
				FRetryTimeOut = MAX_RETRY_SYNCRO;
				Result = GSM_OK;												//	SMS enviado OK
			}else{
				FRetryTimeOut--;
				if(FRetryTimeOut == 0){											//	Controlamos timeout de respuesta
					Result = GSM_TIMEOUT;
					FRetryTimeOut = MAX_RETRY_SYNCRO;
					FGSMProcessStatus = GSM_SEND_SMS_FAIL;
				}
			}
		}else
			Result = GSM_IN_PROGRESS;
		break;
	case	GSM_SEND_SMS_FAIL:													//	Fallo el envio
		Result = GSM_TIMEOUT;
		break;
	case	GSM_SEND_SMS_END:													//	Finalizo el envio del SMS
		Result = GSM_OK;
		break;
	default:
		break;
	}
	return Result;
}

/**
 * 	GSMDriverSetMessage:
 * 		Establece el mensaje a enviar.
 * 	Parametros:
 * 		char *AMessage		Puntero a la cadena de texto que contiene el mensaje
 * */
void GSMDriverSetMessage(char *AMessage)
{
	FMessage = AMessage;							//	Guarda en una variable del modulo el puntero
	FGSMProcessStatus = GSM_SEND_SMS_STEP0;			//	Inicializa la variable de control de la maquina de estados para el envio de mensajes.
#if DEBUG_GSM_DRIVER
	printf("%s \r\n",FMessage);
#endif
}




#endif /* MAIN_GSMDRIVER_C_ */
