/*
 * cpu.h
 *
 *  Created on: 11/06/2014
 *      Author: utnso
 */

#ifndef CPU_H_
#define CPU_H_

#include <parser/metadata_program.h>
#include <parser/parser.h>
#include "funciones.h"
#include <commons/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

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


void salirPorQuantum(int socketKernel, estructura_pcb *pcb);
void parsearUnaInstruccion(char* unaIns);
void parsearUltimaInstruccion(char* ultIns, int socketKernel);
void errorDeProxInstruccion(int socketKernel);
char* solicitarProxSentenciaAUmv(int socket, estructura_pcb *pcb);
t_puntero definirVariable(t_nombre_variable identificador_variable);
t_puntero obtenerPosicionVariable(t_nombre_variable identificador_variable);
t_valor_variable dereferenciar(t_puntero direccion_variable);
void asignar(t_puntero direccion_variable, t_valor_variable valor);
t_valor_variable obtenerValorCompartida(t_nombre_compartida variable);
t_valor_variable asignarValorCompartida(t_nombre_compartida variable, t_valor_variable valor);
void irAlLabel(t_nombre_etiqueta t_nombre_etiqueta);
void llamarSinRetorno(t_nombre_etiqueta etiqueta);
void llamarConRetorno(t_nombre_etiqueta etiqueta, t_puntero donde_retornar);
void finalizar(void);
void retornar(t_valor_variable retorno);
void imprimir(t_valor_variable valor_mostrar);
void imprimirTexto(char* texto);
void entradaSalida(t_nombre_dispositivo dispositivo, int tiempo);
void wait(t_nombre_semaforo identificador_semaforo);
void signal(t_nombre_semaforo identificador_semaforo);


#endif /* CPU_H_ */
