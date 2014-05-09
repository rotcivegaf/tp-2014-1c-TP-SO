/*
 * srvMultipleConHilos.c
 *
 *  Created on: 06/05/2014
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
#include <pthread.h>

#define PUERTO 6667
#define IP "127.0.0.1"
#define BACKLOG 3
#define MAXSIZE 1024

void *administrarConexion(void *socket);

int main(){
	int socketServidor,socketCliente;
	struct sockaddr_in servidor,cliente;

	pthread_t thread[BACKLOG-1];
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED); //Seteo los hilos como detached para no tener que esperar a que un hilo termine para crear otro

	socketServidor=socket(AF_INET, SOCK_STREAM, 0);

	//Seteo de ip, puerto y protocolo del srv
	servidor.sin_family = AF_INET;
	servidor.sin_addr.s_addr= inet_addr(IP);
	memset(&servidor.sin_zero, 0, sizeof servidor.sin_zero[8]);
	servidor.sin_port= htons(PUERTO);

	//Para setear en que puerto e ip va a escuchar el srv
	if(bind(socketServidor,(struct sockaddr*)&servidor,sizeof(struct sockaddr))==-1) {
		printf("error en bind() \n");
		exit(-1);
	}

	if(listen(socketServidor,BACKLOG) == -1) {
		printf("error en listen()\n");
		exit(-1);
	}

	socklen_t sin_size = sizeof(struct sockaddr_in);

	int i = 0;

	while(1){

	if (i==BACKLOG) //Para resetear i si ya llegue al max de conexiones, sino thread[] tiraria error
		i=0;

	socketCliente = accept(socketServidor, (struct sockaddr *)&cliente, &sin_size);
	pthread_create( &thread[i++], &attr, administrarConexion, (void*) &socketCliente);

	}
	return 0;
}

void *administrarConexion(void *socket){
	int sock = *(int*)socket;
	int read_size;
	char message[MAXSIZE];

	while( (read_size = recv(sock , message , MAXSIZE , 0)) > 0 ){
		message[read_size] = '\0';
		printf("Hilo numero %ld", pthread_self());
		printf("%s",message);
		memset(message, 0, MAXSIZE);
	}

	if(read_size == 0)	{
		while(1); //Esto es para que nunca termine el hilo a proposito, queria ver si me dejaba hacer un maximo de conexiones pero no..
		puts("Client disconnected");
	}
	else if(read_size == -1)
		puts("recv failed");

	close(sock);
	return 0;
}

