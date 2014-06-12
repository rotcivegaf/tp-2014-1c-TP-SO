/*
 * funciones.c
 *
 *  Created on: 10/06/2014
 *      Author: utnso
 */

#include "funciones.h"

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
		printf("Error en el seteo del servidor (bind) \n");
		exit(-1);
	}

	return socket_srv;
}

/*
wait(semaforo *s){
	s->valor--;
	if(s->valor<0){
		añadir proceso a s->list;
		block();
	}
}

signal(semaforo *s){
	s->valor++;
	if(s->valor<=0){
		eliminar un proceso p de s->list;
		wakeup();
	}
}*/

