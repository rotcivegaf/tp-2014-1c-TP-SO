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
#include <arpa/inet.h>

#define IP "127.0.0.1" //Esto hay que cargarlo por archivo de config, que esta en la variable de entorno ANSISOP_CONFIG
#define PUERTO 6667


int tamanioArchivo(FILE *archivo);

int main(int argc, char *argv[])
{
	struct sockaddr_in infoConexionKernel;

	infoConexionKernel.sin_family = AF_INET;
	infoConexionKernel.sin_addr.s_addr= inet_addr(IP);
	infoConexionKernel.sin_port = htons(PUERTO);
	memset(&infoConexionKernel.sin_zero, 0, sizeof infoConexionKernel.sin_zero[8]);

	/*t_config *config=config_create("ubicacion?"); 	Para leer el arch de config
	int ipKernel = config_get_int_value(config, "IP");
	int puertoKernel = config_get_int_value(config, "Puerto");*/

	int socketKernel = socket(AF_INET,SOCK_STREAM,0);

	if(connect(socketKernel,(struct sockaddr*)&infoConexionKernel, sizeof(struct sockaddr_in))==-1){
		printf("Error:No se pudo conectar con el kernel \n");
		exit(1);
	}

	FILE *scriptAProcesar = fopen(argv[1],"r");
	int *tamanioScript = malloc(sizeof (int));
	*tamanioScript=tamanioArchivo(scriptAProcesar);
	char script[*tamanioScript];


	/*
	//Envia el tamanio del script
	send(socketKernel,tamanioScript,sizeof (int),0);
	*/

	int i=0;
	//Lee el script
	while(!feof(scriptAProcesar))
		script[i++]=fgetc(scriptAProcesar);

	send(socketKernel, script, *tamanioScript, 0);


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


