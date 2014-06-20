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
#include <commons/string.h>
#include <commons/collections/dictionary.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include "sockets_lib.h"
//importar la shared library



typedef struct{
		u_int32_t id;
		u_int32_t segmento_codigo;
		u_int32_t segmento_stack;
		u_int32_t *cursor_stack;
		u_int32_t indice_codigo;
		u_int32_t indice_etiquetas;
		u_int32_t program_counter;
		u_int32_t tamanio_context;
		u_int32_t tamanio_indice_etiquetas;
		u_int32_t cant_instrucciones;
		u_int32_t tamanio_script;
		u_int32_t peso;
		u_int32_t socket_asociado;
}estructura_pcb;

/*typedef struct{
	char *puertoKernel;
	char *ipKernel;
	char *puertoUmv;
	char *ipUmv;
}t_datos_config;*/

t_dictionary dicVariables;
int socketKernel, socketUmv;
t_men_comun *men;


void handshake_umv();
void handshake_kernel();
void recv_pcb_del_kernel(t_men_comun *men);
void destruirDiccionario();
void signal_handler(int sig);
//t_config *levantarConfiguracion();
void crearDiccionario();
void salirPorQuantum(int socketKernel, estructura_pcb *pcb);
void parsearUnaInstruccion(char* unaIns);
void parsearUltimaInstruccion(char* ultIns, int socketKernel);
void errorDeProxInstruccion(int socketKernel);
char* solicitarProxSentenciaAUmv(int socket, estructura_pcb *pcb);


//primitivas anSISOP
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

//primitivas anSISOP del Kernel
void imprimir(t_valor_variable valor_mostrar);
void imprimirTexto(char* texto);
void entradaSalida(t_nombre_dispositivo dispositivo, int tiempo);
void wait(t_nombre_semaforo identificador_semaforo);
void mi_signal(t_nombre_semaforo identificador_semaforo);


#endif /* CPU_H_ */
