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

#define IP "127.0.0.1"
#define PUERTO "6667"


int tamanioArchivo(FILE *archivo);

int main(int argc, char *argv[])
{
	struct addrinfo hints;
	struct addrinfo *serverInfo;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/*t_config *config=config_create("programa.conf"); 	el arch de config se saca de la variable de entorno ANSISOP_CONFIG, lo cual ni idea
	int ipKernel = config_get_int_value(config, "IP");
	int puertoKernel = config_get_int_value(config, "Puerto");*/

	getaddrinfo(IP, PUERTO, &hints, &serverInfo); 	//Carga los datos de la conexion
	int serversocket = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);

	if(connect(serversocket, serverInfo->ai_addr, serverInfo->ai_addrlen)==-1){
		printf("Error:No se pudo conectar con el kernel");
		exit(1);
	}
	freeaddrinfo(serverInfo); 						//Ya no se necesita la info

	char rutaInterprete[80];
	FILE *scriptAProcesar = fopen(argv[1],"r");
	int tamanioScript = tamanioArchivo(scriptAProcesar);
	char script[tamanioScript];
	int i=0;

	fgets(rutaInterprete,80,scriptAProcesar);
	while(!feof(scriptAProcesar))
		script[i++]=fgetc(scriptAProcesar);

	send(serversocket, script, tamanioScript, 0);

	close(serversocket);
	fclose(scriptAProcesar);
	return 0;
}

int tamanioArchivo(FILE *archivo){
	fseek(archivo, 0L, SEEK_END);
	int tamanio = ftell(archivo);
	fseek(archivo,0L, SEEK_SET);
	return tamanio;
}


