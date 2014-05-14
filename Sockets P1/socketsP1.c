/*
 * socketsP1.c
 *
 *  Created on: 04/05/2014
 *      Author: utnso
 */

//Servidor Basico

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>


#define PUERTO 6667  //El puerto que será abierto
#define BACKLOG 1 //Es la cantidad de conexiones permitidas
#define IP "127.0.0.1"
#define MAXSIZE 1024


int main (){

	int socketServidor,socketCliente;
	struct sockaddr_in servidor,cliente;	//Estructuras en las que se guarda la info de las conexiones

	int msgSize;				//El tamanio del msg que le llega
	char message[MAXSIZE];

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

	listen(socketServidor,BACKLOG);

	socklen_t sin_size = sizeof(struct sockaddr_in);

	socketCliente = accept(socketServidor, (struct sockaddr *)&cliente, &sin_size);

	while((msgSize = recv(socketCliente , message , MAXSIZE , 0))>0){
		message[msgSize] = '\0';		//Para que no imprima caracteres de mas
		printf("%s \n",message);
		//memset(message, 0, MAXSIZE);
	}

	printf("Cliente desconectado");
	close(socketCliente);

	return 0;
}

