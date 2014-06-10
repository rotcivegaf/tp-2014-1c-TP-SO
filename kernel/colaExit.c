/*
 * colaExit.c
 *
 *  Created on: 10/06/2014
 *      Author: utnso
 */

#include <commons/collections/list.h>
#include "planificadores.h"


void *colaExit(){

	extern t_list *cola_exit;
	extern int grado_multiprog;

	if (!list_is_empty(cola_exit)){
		estructura_pcb *pcb= list_remove(cola_exit,1);
		pedirDestruirSegmentos(pcb->id,socketUmv);
		//destruirpcb
		grado_multiprog--;
	}

	return 0;
}


