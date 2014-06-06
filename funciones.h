/*
 * funciones.h
 *
 *  Created on: 27/05/2014
 *      Author: utnso
 */

#ifndef FUNCIONES_H_
#define FUNCIONES_H_

#include <stdio.h>
#include <string.h>		//para el memset
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>		//para el close
#include <arpa/inet.h>	//para el inet_addr


//Se le manda la ip y el puerto al que se quiere conectar y devuelve el socket
int conectarse(char* ip,int puerto){
	struct sockaddr_in infoConexion;

		infoConexion.sin_family = AF_INET;
		infoConexion.sin_addr.s_addr= inet_addr(ip);
		infoConexion.sin_port = htons(puerto);
		memset(&infoConexion.sin_zero, 0, sizeof infoConexion.sin_zero[8]);

		int sock = socket(AF_INET,SOCK_STREAM,0);

		if(connect(sock,(struct sockaddr*)&infoConexion, sizeof(struct sockaddr_in))==-1){
			printf("Error:No se pudo conectar \n");
			exit(1);
		}
		return sock;
}

int servidor(char *ip, int puerto){
	int socket_srv;
	struct sockaddr_in server;

	socket_srv=socket(AF_INET, SOCK_STREAM, 0);

	//Seteo de ip, puerto y protocolo del srv
	server.sin_family = AF_INET;
	server.sin_addr.s_addr= inet_addr(ip);
	memset(&server.sin_zero, 0, sizeof server.sin_zero[8]);
	server.sin_port= htons(puerto);

	//Para setear en que puerto e ip va a escuchar el srv
	if(bind(socket_srv,(struct sockaddr*)&server,sizeof(struct sockaddr))==-1) {
		printf("error en bind() \n");
		exit(-1);
	}

	return socket_srv;
}

#endif /* FUNCIONES_H_ */

