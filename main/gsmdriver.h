/*
 * Modulo gsmdriver.h
 *	Este modulo implementa un driver para el control del modulo GSM a traves de la UART.
 *	Controla la deteccion, inicializacion , configuracion y el envio de mensajes.
 *
 */

#ifndef MAIN_GSMDRIVER_H_
#define MAIN_GSMDRIVER_H_

enum ProcessState{GSM_OK, GSM_TIMEOUT, GSM_IN_PROGRESS};
enum ConfigureProcessState{GSM_CONFIGURE_OK, GSM_CONFIGURE_TIMEOUT, GSM_CONFIGURE_IN_PROGRESS};

/**
 * 	GSMDriverInit:
 * 		Inicializa el modulo
 * */
void GSMDriverInit(void);
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
uint32_t GSMDriverStartProcess(void);
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
uint32_t GSMDriverConfigureProcess(void);
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
uint32_t GSMDriverSendSMS(void);
/**
 * 	GSMDriverSetMessage:
 * 		Establece el mensaje a enviar.
 * 	Parametros:
 * 		char *AMessage		Puntero a la cadena de texto que contiene el mensaje
 * */
void GSMDriverSetMessage(char *AMessage);

#endif /* MAIN_GSMDRIVER_H_ */
