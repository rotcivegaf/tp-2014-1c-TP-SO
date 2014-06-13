/*
 * mockUmv.c
 *
 *  Created on: 06/06/2014
 *      Author: utnso
 */

#include "funciones.h"
#include <stdlib.h>

#define IP "127.0.0.1"
#define PUERTO 5002
#define BACKLOG 1

int main(){

	int socket = servidor(IP,PUERTO);

	listen(socket,BACKLOG);
	socklen_t sin_size = sizeof(struct sockaddr_in);
	struct sockaddr_in kernel;
printf("escuchando \n");
	int socketKernel = accept(socket, (struct sockaddr *)&kernel, &sin_size);
printf("conexion aceptada \n");
	int idMensaje;
	recv(socketKernel,&idMensaje,sizeof(int),0);
printf("id mensaje %d \n",idMensaje);

	int id_programa;
	int tamanio_segmento;

	recv(socketKernel,&id_programa,sizeof(int),0);
printf("id programa %d \n",id_programa);
/*
	recv(socketKernel,datos,sizeof(int)*2,0);
	memcpy(&id_programa,datos,sizeof(int));
	memcpy(&tamanio_segmento,datos+sizeof(int),sizeof(int));
*/
	int direccion = 400;

//printf("tamanio: %d \n",tamanio_segmento);
	send(socketKernel,&direccion,sizeof(int),0);

	return 0;

}




