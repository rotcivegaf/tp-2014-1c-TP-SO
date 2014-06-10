/*
 * planificadores.h
 *
 *  Created on: 23/05/2014
 *      Author: utnso
 */

#ifndef PLANIFICADORES_H_
#define PLANIFICADORES_H_

#include "funciones.h"

typedef struct{
		int id;
		int *segmento_codigo;
		int *segmento_stack;
		int *cursor_stack;
		int *indice_codigo;
		int *indice_etiquetas;
		int program_counter;
		int tamanio_context;
		int tamanio_indice_etiquetas;
		int tamanio_instrucciones;
		int tamanio_script;
		int peso;
		int socket_asociado;
}estructura_pcb;

void *plp();
void *pcp();
void *colaExit();

#endif /* PLANIFICADORES_H_ */


