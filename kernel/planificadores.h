/*
 * planificadores.h
 *
 *  Created on: 23/05/2014
 *      Author: utnso
 */

#ifndef PLANIFICADORES_H_
#define PLANIFICADORES_H_

#include "funciones.h"
#include<parser/metadata_program.h>

typedef struct{
		int id;
		int segmento_codigo;
		int segmento_stack;
		int *cursor_stack;
		int indice_codigo;
		int indice_etiquetas;
		int program_counter;
		int tamanio_context;
		int tamanio_indice_etiquetas;
		int cant_instrucciones;
		int tamanio_script;
		int peso;
		int socket_asociado;
}estructura_pcb;

void *plp();
void *pcp();
void *colaExit();

estructura_pcb* crearPCB(t_metadata_program *metadata, int script_size, int socket);
int calcularPeso(t_metadata_program *metadata);
int pedirSegmentos(estructura_pcb *pcb, int socketUmv);
int pedirSegmento(int idPrograma, int tamanio, int socket);
int pedirDestruirSegmentos(int id_programa,int socket);
int avisarNoHayEspacio(int socketPrograma);
int guardarContenido(int direccion, int tamanio, void *contenido, int socketUmv);
bool comparar_peso(estructura_pcb *pcb1,estructura_pcb *pcb2);
void mostrar_ready(t_list *ready);
void imprimirIdPcb(estructura_pcb *pcb);
int indicePcbConId(t_list *lista,int id);

#endif /* PLANIFICADORES_H_ */


