/**
 * 	Modulo define.h
 *		Este modulo se usa para poner declaraciones comunes, para hacerlas disponibles en los
 *		modulos que se necesite.
 *
 */

#ifndef MAIN_DEFINE_H_
#define MAIN_DEFINE_H_

/*** Eventos del sistema ***/
typedef enum
{
	GSM_DEVICE_NOT_DETECTED,		//	Modulo GSM no respondio despues de un timeout determinado
	GSM_DEVICE_CONFIGURE_FAIL,		//	Fallo la configuracion del modulo GSM
	GSM_DEVICE_INIT_OK,				//	Modulo GSM inicializado, configurado y registrado en la red
	GSM_DEVICE_SEND_SMS_OK,			//	SMS enviado OK
	GSM_DEVICE_SEND_SMS_FAIL,		//	Falla el envio del SMS
	SAM_DEVICE_OK,					//	SAMD21 OK
	SAM_DEVICE_NOT_DETECTED,		//	SAMD21 no responde al comando de exploracion
	SAM_MESSAGE_READY				//	Se recibio un mensaje desde el SAMD21
}TypeEventId;

typedef struct
{
	TypeEventId	EventID;	//	Tipo de evento
	void		*Data;		//	Puntero generico para pasar datos si es necesario
}TSystemEvent;

#define CELPHONE_NUMBER		"\"+543513024449\"\r"


#endif /* MAIN_DEFINE_H_ */
