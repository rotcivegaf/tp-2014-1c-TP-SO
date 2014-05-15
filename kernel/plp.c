/*
 * plp.c
 *
 *  Created on: 13/05/2014
 *      Author: utnso
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <parser/metadata_program.h>

#define PUERTO 6667  		//El puerto que será abierto
#define BACKLOG 1 			//Es la cantidad de conexiones permitidas
#define IP "127.0.0.1"
#define MAXSIZE 1024


int main(){

	int socketKernel,socketPrograma;
	struct sockaddr_in kernel,programa;	//Estructuras en las que se guarda la info de las conexiones

	socketKernel=socket(AF_INET, SOCK_STREAM, 0);

	//Seteo de ip, puerto y protocolo del srv
	kernel.sin_family = AF_INET;
	kernel.sin_addr.s_addr= inet_addr(IP);
	memset(&kernel.sin_zero, 0, sizeof kernel.sin_zero[8]);
	kernel.sin_port= htons(PUERTO);

	//Para setear en que puerto e ip va a escuchar el srv
	if(bind(socketKernel,(struct sockaddr*)&kernel,sizeof(struct sockaddr))==-1) {
		printf("error en bind() \n");
		exit(-1);
	}

	listen(socketKernel,BACKLOG);

	socklen_t sin_size = sizeof(struct sockaddr_in);
	socketPrograma = accept(socketKernel, (struct sockaddr *)&programa, &sin_size);

	/*
	int *tamanioScript=malloc(sizeof (int));
	recv(socketPrograma,tamanioScript,sizeof (int),0);
	printf("%d",*tamanioScript);
	*/

	char *script=malloc(MAXSIZE); //el MAXSIZE deberia cambiarse por la variable tamanioScript

	recv(socketPrograma , script , MAXSIZE , 0);

	t_medatada_program* metadata;
	metadata = metadata_desde_literal(script);
	//Esto es de prueba, metadata hay que mandarlo a una funcion para que cree el pcb por ejemplo
	printf("Cantidad de etiquetas %i\n",metadata->cantidad_de_etiquetas);
	printf("Cantidad de funciones %i\n",metadata->cantidad_de_funciones);
	printf("Etiquetas %s\n",metadata->etiquetas);
	printf("Tamaño del mapa serializado de etiquetas %i\n",metadata->etiquetas_size);
	printf("Tamaño del mapa serializado de instrucciones %i\n",metadata->instrucciones_size);
	printf("El numero de la primera instruccion es %i\n",metadata->instruccion_inicio);

	close(socketPrograma);
	close(socketKernel);
	return 0;

}


