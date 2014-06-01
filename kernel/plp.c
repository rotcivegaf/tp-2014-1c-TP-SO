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
#include <commons/collections/list.h>
#include "planificadores.h"

#define BACKLOG 1 			//Es la cantidad de conexiones permitidas
#define IP "127.0.0.1"
#define MAXSIZE 1024

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
		char* etiquetas;				//Dsp va en el segmento indice de etiquetas
		t_intructions*	instrucciones_serializado;  //Dsp va en el segmento indice de codigo
		int cant_lineas_codigo;
		int peso;
}estructura_pcb;

estructura_pcb* crearPCB(char *script);
int calcular_peso(t_metadata_program *metadata, int cant_lineas_cod);

void *plp(){
	/*bool comparar_peso(estructura_pcb *pcb1,estructura_pcb *pcb2){
		return (pcb1->peso < pcb2->peso);
	}*/

	t_list *cola_new = list_create();

	extern int puerto_programas;
	int socketKernel,socketPrograma;
	struct sockaddr_in kernel,programa;	//Estructuras en las que se guarda la info de las conexiones

	socketKernel=socket(AF_INET, SOCK_STREAM, 0);

	//Seteo de ip, puerto y protocolo del srv
	kernel.sin_family = AF_INET;
	kernel.sin_addr.s_addr= inet_addr(IP);
	memset(&kernel.sin_zero, 0, sizeof kernel.sin_zero[8]);
	kernel.sin_port= htons(puerto_programas);

	//Para setear en que puerto e ip va a escuchar el kernel
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

	estructura_pcb *pcb=crearPCB(script);
	list_add(cola_new, pcb);
	//list_sort(cola_new,comparar_peso);

	close(socketPrograma);
	close(socketKernel);
	free(script);
	free(cola_new);
	return 0;
}

estructura_pcb* crearPCB(char *script){
	estructura_pcb *pcb=malloc(sizeof(estructura_pcb));
	t_medatada_program *metadata = metadata_desde_literal(script);

	pcb->program_counter = metadata->instruccion_inicio;
	pcb->tamanio_indice_etiquetas = metadata->etiquetas_size;
	pcb->etiquetas=metadata->etiquetas;
	pcb->instrucciones_serializado = metadata->instrucciones_serializado;
	pcb->cant_lineas_codigo = metadata->instrucciones_size; 		//Suponiendo que la cant de lineas de codigo sea la cant de instrucc
	pcb->peso = calcular_peso(metadata,pcb->cant_lineas_codigo);

	return pcb;
}

int calcular_peso(t_metadata_program *metadata, int cant_lineas_codigo){
	return (5 * (metadata->cantidad_de_etiquetas) + 3 * (metadata->cantidad_de_funciones) + cant_lineas_codigo);
}

