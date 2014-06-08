/*
 * plp.c
 *
 *  Created on: 13/05/2014
 *      Author: utnso
 */

#include <parser/metadata_program.h>
#include <commons/collections/list.h>
#include "planificadores.h"
#include "funciones.h"

#define BACKLOG 1 			//Es la cantidad de conexiones permitidas
#define IP "127.0.0.1"

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
		int peso;
		int socket_asociado;
}estructura_pcb;

estructura_pcb* crearPCB(t_medatada_program *metadata, int socket);
int calcularPeso(t_medatada_program *metadata);
int pedirSegmento(int idPrograma, int tamanio, int socket);

void *plp(){
	/*bool comparar_peso(estructura_pcb *pcb1,estructura_pcb *pcb2){
		return (pcb1->peso < pcb2->peso);
	}*/

	t_list *cola_new = list_create();

	extern int puerto_programas,puerto_umv;
	int socketKernel,socketPrograma,socketUmv;
	struct sockaddr_in programa;	//Estructuras en las que se guarda la info de las conexiones

	socketKernel = servidor(IP,puerto_programas);
	socketUmv = conectarse(IP,puerto_umv);

	listen(socketKernel,BACKLOG);
	socklen_t sin_size = sizeof(struct sockaddr_in);
	socketPrograma = accept(socketKernel, (struct sockaddr *)&programa, &sin_size);

	//Recepcion y deserializacion del script (quizas convenga hacerlo una funcion)
	int *script_size = malloc(sizeof(int));
	recv(socketPrograma , script_size , sizeof(int) , 0);
	char *script = malloc(*script_size);
	recv(socketPrograma , script , *script_size , 0);

	t_medatada_program *metadata = metadata_desde_literal(script);
	estructura_pcb *pcb=crearPCB(metadata,socketPrograma);

	//Pedir segmentos a la umv
	//Si se pudieron pedir los 4:
	//Envio el contenido de cada uno a la umv para que los guarde
	//Agrego el pcb a la cola de new:
		//list_add(cola_new, pcb);
		//list_sort(cola_new,comparar_peso);
	//Si el grado de multiprog me deja, lo paso de new a ready

	close(socketKernel);
	//free(cola_new);
	return 0;
}

estructura_pcb* crearPCB(t_medatada_program *metadata, int socket){
	estructura_pcb *pcb=malloc(sizeof(estructura_pcb));

	pcb->program_counter = metadata->instruccion_inicio;
	pcb->tamanio_indice_etiquetas = metadata->etiquetas_size;
	pcb->peso = calcularPeso(metadata);
	pcb->socket_asociado = socket;

	return pcb;
}

int calcularPeso(t_medatada_program *metadata){
	return (5 * (metadata->cantidad_de_etiquetas) + 3 * (metadata->cantidad_de_funciones) + (metadata->instrucciones_size));
}

int pedirSegmento(int id_programa, int tamanio, int socket){
	int *pedido = malloc(sizeof(int) *3);
	int id_mensaje = 1;
	int direcc_segmento;

	memcpy(pedido,&id_mensaje,sizeof(id_mensaje));
	memcpy(pedido+sizeof(id_mensaje),&id_programa,sizeof(id_programa));
	memcpy(pedido+sizeof(id_mensaje)+sizeof(id_programa),&tamanio,sizeof(tamanio));

	send(socket,pedido,sizeof(int)*3,0);
	recv(socket,&direcc_segmento,sizeof(int),0);

	return direcc_segmento;
}

int pedirDestruirSegmentos(int id_programa,int socket){
	int *pedido = malloc(sizeof(int));
	*pedido = 2;
	send(socket,pedido,sizeof(int),0);
	return 0;
}

