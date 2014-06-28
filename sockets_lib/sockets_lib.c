#include "sockets_lib.h"

/* crea un socket, lo bindea a un puerto y hace un listen con un maximo de 5 conecciones
 * retorna el socket conectado
 */
int socket_crear_server(char *puerto){
	struct addrinfo hints;
	struct addrinfo *serverInfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;

	getaddrinfo(NULL, puerto, &hints, &serverInfo);
	int soc = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
	if (soc == -1){
		perror("socket server");
		exit(1);
	}
	int resultado_bind = bind(soc ,serverInfo->ai_addr, serverInfo->ai_addrlen);
	if (resultado_bind == -1){
		perror("connect");
		exit(1);
	}
	int resultado_escucha = listen(soc, 5);
	if (resultado_escucha == -1){
		perror("listen");
		exit(1);
	}
	freeaddrinfo(serverInfo);
	return soc;
}

/*recibe un socket conectado, escuchando un puerto, acepta una coneccion
 *retorna un nuevo socket, conectado
 */
int socket_accept(int socket_server){
	struct sockaddr_storage addr;
	socklen_t len = sizeof addr;
	getpeername(socket_server, (struct sockaddr*)&addr, &len);
	int ret_fd = accept(socket_server, (struct sockaddr *) &addr, &len);
	if (ret_fd == -1)
		perror("accept");
	return ret_fd;
}

/*crea un socket y lo conecta
 *retorna un socket conectado
 */
int socket_crear_client(char *puerto, char *ip){
	struct addrinfo hints;
	struct addrinfo *serverInfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	getaddrinfo(ip, puerto, &hints, &serverInfo);
	int socket_client = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
	if (socket_client == -1){
		perror("socket cliente");
		exit(1);
	}
	int resultado_coneccion = connect(socket_client, serverInfo->ai_addr, serverInfo->ai_addrlen);
	if (resultado_coneccion == -1){
		perror("connect");
		exit(1);
	}
	freeaddrinfo(serverInfo);
	return socket_client;
}

/*cierra el socket y maneja el error
 */
void socket_cerrar(int soc){
	if (close(soc) == -1){
		perror("close");
		exit(1);
	}
}

/*
 * recibe tipo de mensaje, un dato y el tamanio del "dato"
 * retorna un t_men_comun
 */
t_men_comun *crear_men_comun(int32_t tipo, char *dato, int32_t tam_dato){
	t_men_comun *mensaje = malloc(sizeof(t_men_comun));
	mensaje->tipo = tipo;
	mensaje->tam_dato = tam_dato;
	if (dato == NULL){
		mensaje->dato = malloc(sizeof(char));
		mensaje->dato = "";
	}else{
		mensaje->dato = malloc(tam_dato);
		mensaje->dato = dato;
	}
	return mensaje;
}

/*envia un mensaje comun al socket conectado
 *los mensajes comunes pueden ser de cualquier tipo y tamanio, y tienen un solo dato
 *retorna los bytes que pudo mandar
 */
int socket_send_comun(int soc,t_men_comun *men){
	char *stream = men_serealizer_comun(men);
	int32_t length;
	memcpy(&length, stream, sizeof(int32_t));
	int pude_enviar = send(soc, stream, length, 0);
	if (pude_enviar == -1){
		perror("send");
		exit(1);
	}
	if (pude_enviar != length)
		printf("NO PUDE MANDAR TODO\n");
	free(stream);
	return pude_enviar;
}

/*recibe un mensaje comun del socket conectado
 *los mensajes comunes pueden ser de cualquier tipo y tamanio, y tienen un solo dato
 *retorna el mensaje recibido o
 *retorna un mensaje con tipo CONEC_CERRADA, si se desconecta el socket
 */
t_men_comun *socket_recv_comun(int soc){
	int32_t length = 0;
	char *aux_len = malloc(sizeof(int32_t));
	int resultado_recv = recv(soc, aux_len, sizeof(int32_t), MSG_PEEK);
	if (resultado_recv == -1){
		perror("recv");
		exit(1);
	}
	if (resultado_recv == 0)
		return crear_men_comun(CONEC_CERRADA,NULL,0);
	memcpy(&length, aux_len, sizeof(int32_t));
	free(aux_len);
	char stream[length];
	resultado_recv = recv(soc, stream, length, MSG_WAITALL);
	if (resultado_recv == -1){
		perror("recv");
		exit(1);
	}
	if (resultado_recv == 0)
		return crear_men_comun(CONEC_CERRADA,NULL,0);
	if (resultado_recv != length)
		printf("NO PUDE RECIBIR TODO\n");
	t_men_comun *mensaje = men_deserealizer_comun(stream);
	return mensaje;
}

/*serealiza un mensaje comun
 * retorna un char * serealizado
 */
char *men_serealizer_comun(t_men_comun *self){
	int length = (sizeof(int32_t)*3)+self->tam_dato;//lenght+tipo+tam del dato+(tam_dato)
	int32_t offset = 0, tmp_size = 0;
	char *stream = malloc(length);
	memcpy(stream, &length, tmp_size = sizeof(int32_t));
	offset = tmp_size;
	memcpy(stream+offset, &self->tipo, tmp_size = sizeof(int32_t));
	offset = offset+tmp_size;
	memcpy(stream+offset, &self->tam_dato, tmp_size = sizeof(int32_t));
	offset = offset+tmp_size;
	memcpy(stream + offset, self->dato, tmp_size = self->tam_dato);

	return stream;
}

/*deserealiza stream comun
 *retorna un t_men_comun
 */
t_men_comun *men_deserealizer_comun(char *stream){
	t_men_comun *self = malloc(sizeof(t_men_comun));
	int32_t offset = sizeof(int32_t), tmp_size = 0;

	memcpy(&self->tipo, stream+offset, tmp_size = sizeof(int32_t));
	offset = offset + tmp_size;
	memcpy(&self->tam_dato, stream+offset, tmp_size = sizeof(int32_t));
	offset = offset+ tmp_size;
	self->dato = malloc(self->tam_dato);
	if (self->tam_dato == 0){
		self->dato = NULL;
		return self;
	}
	memcpy(self->dato, stream + offset, self->tam_dato);

	return self;
}

/* recibe tipo de mensaje, un quantum y un pcb
 * retorna un t_men_quantum_pcb
 */
t_men_quantum_pcb *crear_men_quantum_pcb(int32_t tipo, int32_t quantum, t_pcb *pcb){
	t_men_quantum_pcb *men_quantum_pcb = malloc(sizeof(t_men_quantum_pcb));
	men_quantum_pcb->tipo = tipo;
	men_quantum_pcb->quantum = quantum;
	men_quantum_pcb->pcb =pcb;
	return men_quantum_pcb;
}

/*envia un mensaje con el quantum y el pcb al socket conectado
 *retorna los bytes que pudo mandar
 */
int socket_send_quantum_pcb(int soc,t_men_quantum_pcb *men){
	char *stream = men_serealizer_quantum_pcb(men);
	int32_t length;

	memcpy(&length, stream, sizeof(int32_t));
	int pude_enviar = send(soc, stream, length, 0);
	if (pude_enviar == -1){
		perror("send");
		exit(1);
	}
	if (pude_enviar != length)
		printf("NO PUDE MANDAR TODO\n");
	free(stream);
	return pude_enviar;
}

/*recibe un mensaje quantum+pcb del socket conectado
 *retorna el mensaje recibido o
 *retorna un mensaje con tipo CONEC_CERRADA, si se desconecta el socket
 */
t_men_quantum_pcb *socket_recv_quantum_pcb(int soc){
	int32_t length = 0;
	char *aux_len = malloc(sizeof(int32_t));
	int resultado_recv = recv(soc, aux_len, sizeof(int32_t), MSG_PEEK);
	if (resultado_recv == -1){
		perror("recv");
		exit(1);
	}
	if (resultado_recv == 0)
		return crear_men_quantum_pcb(CONEC_CERRADA, 0, NULL);
	memcpy(&length, aux_len, sizeof(int32_t));
	free(aux_len);
	char stream[length];
	resultado_recv = recv(soc, stream, length, MSG_WAITALL);
	if (resultado_recv == -1){
		perror("recv");
		exit(1);
	}
	if (resultado_recv == 0)
		return crear_men_quantum_pcb(CONEC_CERRADA, 0, NULL);
	if (resultado_recv != length)
		printf("NO PUDE RECIBIR TODO\n");
	t_men_quantum_pcb *men = men_deserealizer_quantum_pcb(stream);
	return men;
}

/*serealiza un mensaje quantum_pcb
 * retorna un char * serealizado
 */
char *men_serealizer_quantum_pcb(t_men_quantum_pcb *self){
	int length = sizeof(int32_t)*13;//length total del mensaje+tipo+quantum+10int (por el pcb)
	int32_t offset = 0, tmp_size = 0;
	char *stream = malloc(length);

	memcpy(stream, &length, tmp_size = sizeof(int32_t));
	offset = tmp_size;
	memcpy(stream+offset, &self->tipo, tmp_size = sizeof(int32_t));//empieso desde el 4 para reserbar el length
	offset = offset+tmp_size;
	memcpy(stream+offset, &self->quantum, tmp_size = sizeof(int32_t));
	offset = offset+tmp_size;
	memcpy(stream+offset, &self->pcb->cant_var_contexto_actual , tmp_size = sizeof(int32_t));
	offset = offset+tmp_size;
	memcpy(stream+offset, &self->pcb->dir_primer_byte_umv_contexto_actual, tmp_size = sizeof(int32_t));
	offset = offset+tmp_size;
	memcpy(stream+offset, &self->pcb->dir_primer_byte_umv_indice_codigo, tmp_size = sizeof(int32_t));
	offset = offset+tmp_size;
	memcpy(stream+offset, &self->pcb->dir_primer_byte_umv_indice_etiquetas	, tmp_size = sizeof(int32_t));
	offset = offset+tmp_size;
	memcpy(stream+offset, &self->pcb->dir_primer_byte_umv_segmento_codigo	, tmp_size = sizeof(int32_t));
	offset = offset+tmp_size;
	memcpy(stream+offset, &self->pcb->dir_primer_byte_umv_segmento_stack, tmp_size = sizeof(int32_t));
	offset = offset+tmp_size;
	memcpy(stream+offset, &self->pcb->id, tmp_size = sizeof(int32_t));
	offset = offset+tmp_size;
	memcpy(stream+offset, &self->pcb->program_counter, tmp_size = sizeof(int32_t));
	offset = offset+tmp_size;
	memcpy(stream+offset, &self->pcb->tam_indice_etiquetas	, tmp_size = sizeof(int32_t));
	offset = offset+tmp_size;
	memcpy(stream+offset, &self->pcb->cant_instrucciones	, tmp_size = sizeof(int32_t));

	return stream;
}

/*deserealiza stream (quantum+pcb)
 *retorna un t_men_quantum_pcb
 */
t_men_quantum_pcb *men_deserealizer_quantum_pcb(char *stream){
	t_men_quantum_pcb *self = malloc(sizeof(t_men_quantum_pcb));
	self->pcb = malloc(sizeof(t_pcb));
	int32_t offset = sizeof(int32_t), tmp_size = 0;

	memcpy(&self->tipo, stream+offset, tmp_size = sizeof(int32_t));
	offset = offset+tmp_size;
	memcpy(&self->quantum, stream+offset, tmp_size = sizeof(int32_t));
	offset = offset+tmp_size;
	memcpy(&self->pcb->cant_var_contexto_actual , stream+offset, tmp_size = sizeof(int32_t));
	offset = offset+ tmp_size;
	memcpy(&self->pcb->dir_primer_byte_umv_contexto_actual , stream+offset, tmp_size = sizeof(int32_t));
	offset = offset+ tmp_size;
	memcpy(&self->pcb->dir_primer_byte_umv_indice_codigo , stream+offset, tmp_size = sizeof(int32_t));
	offset = offset+ tmp_size;
	memcpy(&self->pcb->dir_primer_byte_umv_indice_etiquetas , stream+offset, tmp_size = sizeof(int32_t));
	offset = offset+ tmp_size;
	memcpy(&self->pcb->dir_primer_byte_umv_segmento_codigo , stream+offset, tmp_size = sizeof(int32_t));
	offset = offset+ tmp_size;
	memcpy(&self->pcb->dir_primer_byte_umv_segmento_stack , stream+offset, tmp_size = sizeof(int32_t));
	offset = offset+ tmp_size;
	memcpy(&self->pcb->id , stream+offset, tmp_size = sizeof(int32_t));
	offset = offset+ tmp_size;
	memcpy(&self->pcb->program_counter , stream+offset, tmp_size = sizeof(int32_t));
	offset = offset+ tmp_size;
	memcpy(&self->pcb->tam_indice_etiquetas , stream+offset, tmp_size = sizeof(int32_t));
	offset = offset+ tmp_size;
	memcpy(&self->pcb->cant_instrucciones , stream+offset, tmp_size = sizeof(int32_t));

	return self;
}

/*
 * recibe tipo de mensaje, una base un offset, el tamanio a almacenar, buffer a almacenar("0" si es un solicitud de bytes)
 * retorna un t_men_cpu_umv
 */
t_men_cpu_umv *crear_men_cpu_umv(int32_t tipo, int32_t base, int32_t offset, int32_t tam, char *buffer){
	t_men_cpu_umv *men = malloc(sizeof(t_men_cpu_umv));
	men->tipo = tipo;
	men->base = base;
	men->offset = offset;
	men->tam = tam;
	men->buffer = malloc(tam);
	men->buffer = buffer;
	return men;
}

/*envia un mensaje para almacenar un buffer en la memoria a la umv
 *retorna los bytes que pudo mandar
 */
int socket_send_cpu_umv(int soc,t_men_cpu_umv *men){
	char *stream = men_serealizer_cpu_umv(men);
	int32_t length;
	memcpy(&length, stream, sizeof(int32_t));
	int pude_enviar = send(soc, stream, length, 0);
	if (pude_enviar == -1){
		perror("send");
		exit(1);
	}
	if (pude_enviar != length)
		printf("NO PUDE MANDAR TODO\n");
	free(stream);
	return pude_enviar;
}

/*recibe un mensaje para almacenar un buffer en la memoria de la umv
 *retorna el mensaje recibido o
 *retorna un mensaje con tipo CONEC_CERRADA, si se desconecta el socket
 */
t_men_cpu_umv *socket_recv_cpu_umv(int soc){
	int32_t length = 0;
	char *aux_len = malloc(sizeof(int32_t));
	int resultado_recv = recv(soc, aux_len, sizeof(int32_t), MSG_PEEK);
	if (resultado_recv == -1){
		perror("recv");
		exit(1);
	}
	if (resultado_recv == 0)
		return crear_men_cpu_umv(CONEC_CERRADA,0,0,0,NULL);
	memcpy(&length, aux_len, sizeof(int32_t));
	free(aux_len);
	char stream[length];
	resultado_recv = recv(soc, stream, length, MSG_WAITALL);
	if (resultado_recv == -1){
		perror("recv");
		exit(1);
	}
	if (resultado_recv == 0)
		return crear_men_cpu_umv(CONEC_CERRADA,0,0,0,NULL);
	if (resultado_recv != length)
		printf("NO PUDE RECIBIR TODO\n");
	t_men_cpu_umv *men = men_deserealizer_cpu_umv(stream);
	return men;
}

/*serealiza un mensaje de solicitud de almacenamiento de bytes
 * retorna un char * serealizado
 */
char *men_serealizer_cpu_umv(t_men_cpu_umv *self){
	int length = (sizeof(int32_t)*5)+self->tam;//lenght+tipo+base+offset+tam = 5
	int32_t offset = 0, tmp_size = 0;
	char *stream = malloc(length);

	memcpy(stream, &length, tmp_size = sizeof(int32_t));
	offset = tmp_size;
	memcpy(stream+offset, &self->tipo, tmp_size = sizeof(int32_t));
	offset = offset +tmp_size;
	memcpy(stream+offset, &self->base, tmp_size = sizeof(int32_t));
	offset = offset+tmp_size;
	memcpy(stream+offset, &self->offset, tmp_size = sizeof(int32_t));
	offset = offset+tmp_size;
	memcpy(stream+offset, &self->tam, tmp_size = sizeof(int32_t));
	offset = offset+tmp_size;
	memcpy(stream + offset, self->buffer, tmp_size = self->tam);

	return stream;
}

/*deserealiza stream de solicitud de almacenamiento de bytes
 *retorna un t_men_sol_pos_mem
 */
t_men_cpu_umv *men_deserealizer_cpu_umv(char *stream){
	t_men_cpu_umv *self = malloc(sizeof(t_men_cpu_umv));
	int32_t offset = sizeof(int32_t), tmp_size = 0;

	memcpy(&self->tipo, stream+offset, tmp_size = sizeof(int32_t));
	offset = offset + tmp_size;
	memcpy(&self->base, stream+offset, tmp_size = sizeof(int32_t));
	offset = offset + tmp_size;
	memcpy(&self->offset, stream+offset, tmp_size = sizeof(int32_t));
	offset = offset + tmp_size;
	memcpy(&self->tam, stream+offset, tmp_size = sizeof(int32_t));
	offset = offset+ tmp_size;
	self->buffer = malloc(self->tam);
	memcpy(self->buffer, stream + offset, self->tam);

	return self;
}

/*
 * recibe tipo de mensaje, un id de programa y el tamanio del segmento a crear
 * retorna un t_men_seg
 */
t_men_seg *crear_men_seg(int32_t tipo, int32_t id_prog, int32_t tam_seg){
	t_men_seg *men = malloc(sizeof(t_men_seg));
	men->tipo = tipo;
	men->id_prog = id_prog;
	men->tam_seg = tam_seg;
	return men;
}

/*envia un mensaje de pedido de segmento al socket conectado
 *retorna los bytes que pudo mandar
 */
int socket_send_seg(int soc,t_men_seg *men){
	char *stream = men_serealizer_seg(men);
	int32_t length;
	memcpy(&length, stream, sizeof(int32_t));
	int pude_enviar = send(soc, stream, length, 0);
	if (pude_enviar == -1){
		perror("send");
		exit(1);
	}
	if (pude_enviar != length)
		printf("NO PUDE MANDAR TODO\n");
	free(stream);
	return pude_enviar;
}

/*recibe un mensaje de pedido de segmento del socket conectado
 *retorna el mensaje recibido o
 *retorna un mensaje con tipo CONEC_CERRADA, si se desconecta el socket
 */
t_men_seg *socket_recv_seg(int soc){
	int32_t length = 0;
	char *aux_len = malloc(sizeof(int32_t));
	int resultado_recv = recv(soc, aux_len, sizeof(int32_t), MSG_PEEK);
	if (resultado_recv == -1){
		perror("recv");
		exit(1);
	}
	if (resultado_recv == 0)
		return crear_men_seg(CONEC_CERRADA,0,0);
	memcpy(&length, aux_len, sizeof(int32_t));
	free(aux_len);
	char stream[length];
	resultado_recv = recv(soc, stream, length, MSG_WAITALL);
	if (resultado_recv == -1){
		perror("recv");
		exit(1);
	}
	if (resultado_recv == 0)
		return crear_men_seg(CONEC_CERRADA,0,0);
	if (resultado_recv != length)
		printf("NO PUDE RECIBIR TODO\n");
	t_men_seg *mensaje = men_deserealizer_seg(stream);
	return mensaje;
}

/*serealiza un mensaje de pedido de segmento
 * retorna un char * serealizado
 */
char *men_serealizer_seg(t_men_seg *self){
	int length = (sizeof(int32_t)*4);//lenght+tipo+id_prog+tam del sego
	int32_t offset = 0, tmp_size = 0;
	char *stream = malloc(length);
	memcpy(stream, &length, tmp_size = sizeof(int32_t));
	offset = tmp_size;
	memcpy(stream+offset, &self->tipo, tmp_size = sizeof(int32_t));
	offset = offset+tmp_size;
	memcpy(stream+offset, &self->id_prog, tmp_size = sizeof(int32_t));
	offset = offset+tmp_size;
	memcpy(stream + offset, &self->tam_seg, sizeof(int32_t));

	return stream;
}

/*deserealiza stream comun
 *retorna un t_men_seg
 */
t_men_seg *men_deserealizer_seg(char *stream){
	t_men_seg *self = malloc(sizeof(t_men_seg));
	int32_t offset = sizeof(int32_t), tmp_size = 0;

	memcpy(&self->tipo, stream+offset, tmp_size = sizeof(int32_t));
	offset = offset + tmp_size;
	memcpy(&self->id_prog, stream+offset, tmp_size = sizeof(int32_t));
	offset = offset+ tmp_size;
	memcpy(&self->tam_seg, stream+offset, sizeof(int32_t));

	return self;
}
