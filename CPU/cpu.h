/*
 * cpu.h
 *
 *  Created on: 11/06/2014
 *      Author: utnso
 */

#ifndef CPU_H_
#define CPU_H_

#include "parser/metadata_program.h"
#include "parser/parser.h"
#include "commons/config.h"
#include "commons/string.h"
#include "commons/collections/dictionary.h"
#include "commons/txt.h"
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

struct sigaction sa;
volatile sig_atomic_t got_usr1;

void traerIndiceEtiquetas();
void recibirUnPcb();
void handshake_umv(char *puerto, char *ip);
void handshake_kernel(char *puerto, char *ip);
void recv_pcb_del_kernel(t_men_quantum_pcb *msj);
void cambio_PA(int32_t id_proc);
void manejarSegmentationFault();
void signal_handler(int sig);

void crearDiccionario();
void salirPorQuantum();
void parsearUnaInstruccion(char* unaIns);

void errorDeProxInstruccion(int socketKernel);
char* solicitarProxSentenciaAUmv();
void preservarContexto();
void finalizarContexto();
void logear_int(FILE* destino,int32_t un_int);
void logear_char(FILE* destino,char un_char);
void imprimo_config(char *puertoKernel, char *ipKernel, char *puertoUmv, char *ipUmv);

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
