/*
 * plp.c
 *
 *  Created on: 13/05/2014
 *      Author: utnso
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <parser/metadata_program.h>

#include "planificadores.h"

#define BACKLOG 1 			//Es la cantidad de conexiones permitidas
#define IP "127.0.0.1"
#define MAXSIZE 1024

void *crearPCB(char *script);

void *plp(){
	extern int puerto_programas;
	int socketKernel,socketPrograma;
	struct sockaddr_in kernel,programa;	//Estructuras en las que se guarda la info de las conexiones

	socketKernel=socket(AF_INET, SOCK_STREAM, 0);

	//Seteo de ip, puerto y protocolo del srv
	kernel.sin_family = AF_INET;
	kernel.sin_addr.s_addr= inet_addr(IP);
	memset(&kernel.sin_zero, 0, sizeof kernel.sin_zero[8]);
	kernel.sin_port= htons(puerto_programas);

	//Para setear en que puerto e ip va a escuchar el srv
	if(bind(socketKernel,(struct sockaddr*)&kernel,sizeof(struct sockaddr))==-1) {
		printf("error en bind() \n");
		exit(-1);
	}

	listen(socketKernel,BACKLOG);

	socklen_t sin_size = sizeof(struct sockaddr_in);
	socketPrograma = accept(socketKernel, (struct sockaddr *)&programa, &sin_size);

	/*
	int *tamanioScript=malloc(sizeof (int));
	recv(socketPrograma,tamanioScript,sizeof (int),0);
	printf("%d",*tamanioScript);
	*/

	char *script=malloc(MAXSIZE); //el MAXSIZE deberia cambiarse por la variable tamanioScript

	recv(socketPrograma , script , MAXSIZE , 0);

	close(socketPrograma);
	close(socketKernel);

	return 0;
}

void *crearPCB(char *script){
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
	}pcb;

	t_medatada_program* metadata;
	metadata = metadata_desde_literal(script);

	return 0;
}
