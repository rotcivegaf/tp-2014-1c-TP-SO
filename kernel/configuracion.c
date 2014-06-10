/*
 * configuracion.c
 *
 *  Created on: 23/05/2014
 *      Author: utnso
 */

#include<stdio.h>
#include<commons/config.h>
#include<commons/collections/list.h>
#include <pthread.h>
#include "planificadores.h"

	int puerto_programas;
	int puerto_cpu;
	int puerto_umv;
	int quantum;
	int retardo_q;
	int grado_multiprog_max;
	int stack;
	char **var_globales;
	char **ids_semaforos;
	char **valoresIniciales_semaforos;
	char **retardos_io;
	char **ids_io;

	int grado_multiprog = 0;

	t_list *cola_ready;
	t_list *cola_exit;

int main(int argc, char *argv[]){

	t_config *config;
	config=config_create(argv[1]);

	cola_ready = list_create();
	cola_exit = list_create();

	pthread_t threadPLP, threadPCP, threadEXIT;

	puerto_programas=config_get_int_value(config,"PUERTO_PROG");
	puerto_cpu=config_get_int_value(config,"PUERTO_CPU");
	puerto_umv=config_get_int_value(config,"PUERTO_UMV");
	quantum=config_get_int_value(config,"QUANTUM");
	retardo_q=config_get_int_value(config,"RETARDO");
	grado_multiprog_max=config_get_int_value(config,"MULTIPROGRAMACION");
	stack=config_get_int_value(config,"STACK");
	var_globales=config_get_array_value(config,"VAR_GLOBALES");
	ids_semaforos=config_get_array_value(config,"SEMAFOROS");
	valoresIniciales_semaforos=config_get_array_value(config,"VALOR_SEMAFORO");
	retardos_io=config_get_array_value(config,"RETARDO");
	ids_io=config_get_array_value(config,"ID_HIO");

	//printf("%s",valoresIniciales_semaforos[2]);

	pthread_create( &threadPLP, NULL, plp, NULL);
	pthread_create(&threadEXIT,NULL,colaExit,NULL);
	pthread_create( &threadPCP, NULL, pcp, NULL);

	pthread_join(threadPLP, NULL);
	pthread_join(threadPCP, NULL);
	pthread_join(threadEXIT, NULL);

	return 0;
}

