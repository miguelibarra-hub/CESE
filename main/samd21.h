/*
 * Modulo samd21.h
 * 		Este modulo implementa la interface serie con el microcontrolador SAMD21.
 */

#ifndef MAIN_SAMD21_H_
#define MAIN_SAMD21_H_
/**
 * 	SAMD21Init:
 * 		Inicializa el modulo
 * 	Parametros:
 * 		QueueHandle_t AEventQueue			Recibe como parametro el valor de la cola de mensajes
 * 		UBaseType_t APriority		Prioridad de la tarea que controla el modulo
 * */
void SAMD21Init(QueueHandle_t AEventQueue, UBaseType_t APriority);

/**
 * 	SAMD21FreeCommunicationChannel:
 * 		Libera el canal de comunicaciones con el SAMD21 para un nuevo mensaje
 * */
void SAMD21FreeCommunicationChannel(void);



#endif /* MAIN_SAMD21_H_ */
