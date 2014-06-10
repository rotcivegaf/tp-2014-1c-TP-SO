/*
 * pcp.c
 *
 *  Created on: 10/06/2014
 *      Author: utnso
 */
#include "planificadores.h"


void *pcp(){

	/*
	recibir las conexiones de los cpu
	se fija si hay algo en ready para mandarle
	cuando un cpu con un programa en ejec se desconecta, avisar al programa la excepcion y lo manda a exit
	mueve a ready los procesos que hayan terminado su rafaga  de cpu y concluido su i/o
	envia a ejecutar los procesos en ready a los cpu informando el quantum
	mueve a exit todos los programas qe terminan su ejecucion
	cuando termina de ejecutar o termina el quantum,me devuelve los pcb actualizados
	recibe las system calls del cpu y responde en base a eso
	 */
	return 0;
}


