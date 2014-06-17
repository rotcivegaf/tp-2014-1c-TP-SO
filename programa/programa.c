/*
 * programa.c
 *
 *  Created on: 25/04/2014
 *      Author: utnso
 */

#include "sockets_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <commons/config.h>


#define IP "127.0.0.1" //Esto hay que cargarlo por archivo de config, que esta en la variable de entorno ANSISOP_CONFIG
#define PUERTO 5000


int tamanioArchivo(FILE *archivo);
void handshake_kernel(int soc_kernel);

int main(int argc, char *argv[])
{
	/*t_config *config=config_create("configuracion.conf");
	char *ipKernel=config_get_string_value(config, "IP");
	char *puertoKernel = config_get_int_value(config, "PUERTO");*/

	FILE *scriptAProcesar = fopen(argv[1],"r");
	int tamanioScript=tamanioArchivo(scriptAProcesar);
	char *script = malloc(tamanioScript);

	int socket_kernel=socket_crear_client(PUERTO,IP);
	handshake_kernel(socket_kernel);

	int i=0;
	//Lee el script
	while(!feof(scriptAProcesar))
		script[i++]=fgetc(scriptAProcesar);

	t_mensaje *mensaje_cod_prog = crear_t_mensaje(CODIGO_SCRIPT,script,tamanioScript);
	socket_send_serealizado(socket_kernel,mensaje_cod_prog);

	//espero la respuesta del kernel
	int fin_ejecucion = 0;
	t_mensaje *mensaje_recibido;
	while (fin_ejecucion != 1){
		mensaje_recibido = socket_recv_serealizado(socket_kernel);

		switch(mensaje_recibido->tipo){
		case IMPRIMIR_VALOR:
			int valor = atoi(mensaje_recibido->dato);
			printf("%d",valor);
			break;
		case IMPRIMIR_TEXTO:
			printf("%s",mensaje_recibido->dato);
			break;
		case CPU_DESCONEC:
			printf("CPU DESCONECTADA\n");
			fin_ejecucion = 1;
			break;
		case CONEC_CERRADA:
			fin_ejecucion = 1;
			break;
		case FIN_EJECUCION:
			printf("Ejecucion finalizada");
			fin_ejecucion = 1;
			break;
		case MEM_INSUFICIENTE:
			fin_ejecucion = 1;
			printf("Memoria insuficiente\n");
			//todo ERROR
			break;
		default:
			sleep(1);
			printf("%i\n",mensaje_recibido->tipo);
			printf("El tipo de dato recibido es erroneo\n");
			break;
		}
	}

	close(socket_kernel);
	fclose(scriptAProcesar);
	free(script);
	//config_destroy(config);
	return 0;
}

int tamanioArchivo(FILE *archivo){
	fseek(archivo, 0L, SEEK_END);
	int tamanio = ftell(archivo);
	fseek(archivo,0L, SEEK_SET);
	return tamanio;
}

void handshake_kernel(int soc_kernel){
	t_mensaje *handshake = crear_t_mensaje(HS_KERNEL_PROG,"",1);
	socket_send_serealizado(soc_kernel,handshake);

	handshake = socket_recv_serealizado(soc_kernel);

	if(handshake->tipo == HS_KERNEL_PROG)
		printf("KERNEL conectado\n");
}
