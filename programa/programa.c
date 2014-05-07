/*
 * programa.c
 *
 *  Created on: 25/04/2014
 *      Author: utnso
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <commons/config.h>

#define IP "127.0.0.1" //Esto hay que cargarlo por archivo de config, que esta en la variable de entorno ANSISOP_CONFIG
#define PUERTO "6667"


int tamanioArchivo(FILE *archivo);

int main(int argc, char *argv[])
{
	struct addrinfo hints;
	struct addrinfo *kernelInfo;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	/*t_config *config=config_create("ubicacion?"); 	Para leer el arch de config
	int ipKernel = config_get_int_value(config, "IP");
	int puertoKernel = config_get_int_value(config, "Puerto");*/

	getaddrinfo(IP, PUERTO, &hints, &kernelInfo); 	//Carga los datos de la conexion
	int socketKernel = socket(kernelInfo->ai_family, kernelInfo->ai_socktype, kernelInfo->ai_protocol);

	if(connect(socketKernel, kernelInfo->ai_addr, kernelInfo->ai_addrlen)==-1){
		printf("Error:No se pudo conectar con el kernel \n");
		exit(1);
	}
	freeaddrinfo(kernelInfo); 						//Ya no se necesita la info

	FILE *scriptAProcesar = fopen(argv[1],"r");
	int tamanioScript = tamanioArchivo(scriptAProcesar);
	char script[tamanioScript];
	int i=0;

	while(!feof(scriptAProcesar))
	script[i++]=fgetc(scriptAProcesar);

	send(socketKernel, script, tamanioScript, 0);

	close(socketKernel);
	fclose(scriptAProcesar);
	return 0;
}

int tamanioArchivo(FILE *archivo){
	fseek(archivo, 0L, SEEK_END);
	int tamanio = ftell(archivo);
	fseek(archivo,0L, SEEK_SET);
	return tamanio;
}


