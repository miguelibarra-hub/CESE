/**
 * Modulo leds.h
 * 	Este modulo se encarga del control de dos leds, Actividad y Enlace/Datos
 *
 *
 **/

#ifndef MAIN_LEDS_H_
#define MAIN_LEDS_H_

#define PERIODO_500_MS	5
#define PERIODO_1_S		10


enum LedsName{LED_ACTIVITY, LED_LINK, MAX_LEDS_LENGTH};

enum LedsModes{	LED_BLINK,		//	En este modo el led realiza destellos en forma periodica
				LED_ON, 		//	En este modo el led esta siempre prendido
				LED_OFF};		//	En este modo el led esta siempre apagado

/**
 * 	LedsInit:
 * 		Inicializa el modulo.
 *	Parametros
 *		UBaseType_t APriority		Prioridad de la tarea que controla el modulo
 * */
void LedsInit(UBaseType_t APriority);
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
void SetLedMode(uint32_t AMode, uint32_t APeriodo, uint32_t AChannelLed, uint32_t ABlinkyCount);


#endif /* MAIN_LEDS_H_ */
