/*
 * Modulo gsm.h
 * 	Este modulo implementa el manejo del modulo GSM a traves del driver gsm.
 */

#ifndef MAIN_GSM_H_
#define MAIN_GSM_H_

#include "freertos/queue.h"
/*
 * 	GSMInit:
 * 		Inicializa el modulo.
 * 	Parametros:
 * 		QueueHandle_t AEventQueue	Valor del controlador de la cola de mensajes pasado por parametros
 * 		UBaseType_t APriority		Prioridad de la tarea que controla el modulo
 * */
void GSMInit(QueueHandle_t AEventQueue, UBaseType_t APriority);
/**
 *	SetSMStoSend:
 *		Establece el mensaje a enviar y activa el envio.
 *	Parametros:
 *		char *AMessage		puntero al mensaje
 *	Retorna:
 *		0	OK
 *		-1	Falla
 * */
int32_t SetSMStoSend(char *AMessage);

#endif /* MAIN_GSM_H_ */
