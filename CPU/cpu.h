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



/*typedef struct{
	char *puertoKernel;
	char *ipKernel;
	char *puertoUmv;
	char *ipUmv;
}t_datos_config;*/

//Tipos de datos
	typedef u_int32_t t_puntero;
	typedef u_int32_t t_size;
	typedef u_int32_t t_puntero_instruccion;

	typedef char t_nombre_variable;
	typedef int t_valor_variable;

	typedef t_nombre_variable* t_nombre_semaforo;
	typedef t_nombre_variable* t_nombre_etiqueta;
	typedef  t_nombre_variable* t_nombre_compartida;
	typedef  t_nombre_variable* t_nombre_dispositivo;




t_dictionary *dic_Variables;
t_pcb *pcb;
t_dictionary dicVariables;
int socketKernel, socketUmv;
t_men_comun *men;
t_men_quantum_pcb *msj;
t_men_sol_pos_mem *mess;

void recibirUnPcb();
void handshake_umv(char *ip_umv, char *puerto_umv);
void handshake_kernel(char *ip_k, char *puerto_k);
void recv_pcb_del_kernel(t_men_quantum_pcb *msj);

void signal_handler(int sig);

//t_config *levantarConfiguracion();
void crearDiccionario();
void salirPorQuantum();
void parsearUnaInstruccion(char* unaIns);
void parsearUltimaInstruccion(char* ultIns, int socketKernel);
void errorDeProxInstruccion(int socketKernel);
char* solicitarProxSentenciaAUmv();


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
