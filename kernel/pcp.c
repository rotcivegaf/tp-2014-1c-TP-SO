/*
 * pcp.c
 *
 *  Created on: 10/06/2014
 *      Author: utnso
 */
#include<commons/collections/list.h>
#include "planificadores.h"

#define BACKLOG 1 			//Es la cantidad de conexiones permitidas
#define IP "127.0.0.1"


void *pcp(){

	//recibir las conexiones de los cpu

	extern int puerto_cpu;
	extern t_list *cola_ready;
	extern t_list *cola_exit;
	extern int quantum;

	t_list *cola_exec = list_create();
	t_list *cola_block = list_create();

	int socketPCP, socketCPU;
	struct sockaddr_in cpu;	//Estructuras en las que se guarda la info de las conexiones

	socketPCP = servidor(IP,puerto_cpu);

	listen(socketPCP,BACKLOG);
	socklen_t sin_size = sizeof(struct sockaddr_in);
	socketCPU = accept(socketPCP, (struct sockaddr *)&cpu, &sin_size);

	//se fija si hay algo en ready
	//y le envia a ejecutar los procesos en ready a los cpu informando el quantum

	if(!list_is_empty(cola_ready)){
		estructura_pcb *pcb = list_remove(cola_ready,0);
	/*
		char *pedido = malloc(sizeof(estructura_pcb));

		memcpy(pedido,&pcb,sizeof(estructura_pcb));
	*/
		send(socketPCP,pcb,sizeof(estructura_pcb),0);
		send(socketPCP,&quantum,sizeof(int),0);
		list_add(cola_exec,pcb);
	}

	//cuando un cpu con un programa en ejec se desconecta:
	//avisar al programa la excepcion y lo manda a exit

	int id_mensaje;
	recv(socketCPU,&id_mensaje,sizeof(int),0);
	if(id_mensaje==1){
		estructura_pcb *pcb=malloc(sizeof(estructura_pcb));
		recv(socketCPU,pcb,sizeof(estructura_pcb),0);

		int indice = indicePcbConId(cola_exec,pcb->id);

		list_remove(cola_exec,indice);
		list_add(cola_ready,pcb);

	}
	/*mueve a ready los procesos que hayan concluido su i/o
	mueve a exit todos los programas qe terminan su ejecucion
	cuando termina de ejecutar o termina el quantum,me devuelve los pcb actualizados
	recibe las system calls del cpu y responde en base a eso
	 */
	return 0;
}

int indicePcbConId(t_list *lista,int id){
	int indice;

	bool coincideId(estructura_pcb *pcb){
		return (pcb->id==id);
	}
	list_find_element(lista,(void*)coincideId,&indice);

	return indice;
}

