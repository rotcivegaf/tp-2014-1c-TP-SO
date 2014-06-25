/*
 * cpu.c
 *
 *  Created on: 01/06/2014
 *      Author: utnso
 */

#include "cpu.h"

static const int CONTENIDO_VARIABLE = 20;
static const int POSICION_MEMORIA = 0x10;

int quantum;
int quit_sistema = 1;
int socketKernel;
int socketUmv;
char *prox_inst;
//estructura_pcb *pcb;
t_pcb *pcb;

AnSISOP_funciones functions = {
		.AnSISOP_definirVariable		= definirVariable,
		.AnSISOP_obtenerPosicionVariable= obtenerPosicionVariable,
		.AnSISOP_dereferenciar			= dereferenciar,
		.AnSISOP_asignar				= asignar,
		.AnSISOP_obtenerValorCompartida = obtenerValorCompartida,
		.AnSISOP_asignarValorCompartida = asignarValorCompartida,
		.AnSISOP_irAlLabel              = irAlLabel,
		.AnSISOP_llamarSinRetorno       =llamarSinRetorno,
		.AnSISOP_llamarConRetorno       = llamarConRetorno,
		.AnSISOP_finalizar              = finalizar,
		.AnSISOP_retornar               = retornar,
		.AnSISOP_imprimir				= imprimir,
		.AnSISOP_imprimirTexto			= imprimirTexto,
		.AnSISOP_entradaSalida = entradaSalida};

AnSISOP_kernel kernel_functions = {
		.AnSISOP_wait = wait,
		.AnSISOP_signal = mi_signal};



int main(){
	prox_inst = malloc(0);
	t_men_quantum_pcb *msj;
	t_pcb *pcb = malloc(sizeof(t_pcb));

	/*levanta archivo de configuracion para obtener ip y puerto de kernel y umv*/
	//t_datos_config *unaConfig = levantarConfiguracion();

	t_config *unaConfig = config_create("config.conf");
	char *ipKernel=config_get_string_value(unaConfig, "IPKERNEL");
	char *puertoKernel = config_get_string_value(unaConfig, "PUERTOKERNEL");
	char *puertoUmv = config_get_string_value(unaConfig, "PUERTOUMV");
	char *ipUmv=config_get_string_value(unaConfig, "IPKERNEL");


	/*conexion con el kernel*/

	socketKernel = socket_crear_client(puertoKernel, ipKernel);
	handshake_kernel();

	while(1){

		recibirUnPcb();
	}
	//}

	//recv(socketKernel,pcb, sizeof(estructura_pcb),0);	recv(socketKernel, &quantum, sizeof(int), 0);

	//crear diccionario de variables
	crearDiccionario();

	signal(SIGUSR1, signal_handler);

	int cantIns = 0;

	/*conexion con la umv*/
	socketUmv = socket_crear_client(puertoUmv, ipUmv);
	handshake_umv();
	//int socketUmv = conectarse(ipUmv,puertoUmv);

	char* proxInstrucc = solicitarProxSentenciaAUmv(socketUmv,pcb);

	if (strcmp(proxInstrucc, "error") ){
		errorDeProxInstruccion(socketKernel); // le aviso al pcp que la umv me devolvio error al solicitar prox inst
		}else{
			pcb->program_counter ++; // aumento el pc
			if (pcb->program_counter == pcb->cant_instrucciones){ // me fijo si la sentencia a ejecutar es la ultima del script
				parsearUltimaInstruccion(proxInstrucc, socketKernel);}
				else{
					for(;quantum>0;quantum--){
						//ejecuta tantas instrucciones como quantums tenga
						parsearUnaInstruccion(proxInstrucc);
						cantIns ++;
					}
					salirPorQuantum(socketKernel,pcb);


			}

		}


socket_cerrar(socketKernel);
socket_cerrar(socketUmv);

destruirDiccionario();

return 0;

}

/*t_datos_config *levantarConfiguracion(){
	t_config *unaConfig = config_create("config.conf");//debe obtener este archivo por la direcc en la variable de enterno
	t_datos_config *ret = malloc(sizeof(t_datos_config));
	char *ipKernel=config_get_string_value(ret->ipKernel, "IPKERNEL");
	char *puertoKernel = config_get_string_value(ret->puertoKernel, "PUERTOKERNEL");
	char *ipUmv=config_get_string_value(ret->ipUmv, "IPUMV");
	char *puertoUmv = config_get_string_value(ret->puertoUmv, "PUERTOUMV");
	printf("\n\n------------------------------Archivo Config----------------------------------------\n");
	printf("IP Kernel = %s\n", ret->ipKernel);
	printf("Puerto Kernel = %s\n",ret->puertoKernel);
	printf("IP UMV = %s\n", ret->ipUmv);
	printf("Puerto UMV = %s\n", ret->puertoUmv);
	printf("------------------------------------------------------------------------------------\n\n");
	return ret;
}*/

void crearDiccionario(){
	//t_dictionary *dicVariables = dictionary_create();
	//si el tamaño de contexto es distinto de cero
		// for (de tamaño de contexto a 0){
			//pedir a umv, base= stack offset=(i-1)x5 tamanio=1
			//recibir de la umv key
			//dictionary_put(dicVariables,key, stack + (i-1))
	//}
}

void destruirDiccionario(){
	//dictionary_remove_and_destroy(dicVariables, , );
}

void avisarAlPcp(int unCodigo, int socketKernel){
	char *pedido = malloc(sizeof(int));
	u_int32_t id_mensaje = 6;
	memcpy(pedido, &id_mensaje, sizeof(id_mensaje));
	send(socketKernel,pedido,sizeof(int),0);
}

void salirPorQuantum(int socketKernel, estructura_pcb *pcb){
	//aviso al pcp que termino mi quantum
	u_int32_t codigoQuantum =1;
	avisarAlPcp(codigoQuantum, socketKernel);
	//le mando el pcb actualizado;
	send(socketKernel,pcb, sizeof(estructura_pcb),0);

}
void parsearUltimaInstruccion(char* ultIns, int socketKernel){
	analizadorLinea(strdup(ultIns), &functions, &kernel_functions);
	//avisar al pcp que termino el programa
	u_int32_t codigoFinProg = 2;
	avisarAlPcp(codigoFinProg, socketKernel);
}

void parsearUnaInstruccion(char* unaIns){
	analizadorLinea(strdup(unaIns), &functions, &kernel_functions);
}

void errorDeProxInstruccion(int socketKernel){
	//avisar al pcp que hubo un error de en el pedido de la instruccion
	u_int32_t codigoErrorEnUmv = 3;
	avisarAlPcp(codigoErrorEnUmv, socketKernel);
	//mostrar por pantalla que hubo un excepcion
	printf("Ocurrio una excepcion en el pedido a la UMV\n");
	//concluir la ejecucion de un programa

}

char* solicitarProxSentenciaAUmv(int socket, estructura_pcb *pcb){
	//para decir que se esta conectando una conexion tipo cpu
	char *pedido = malloc(sizeof(int));
	u_int32_t id_mensaje = 4; //para saber que le estoy pidiendo prox instruccion a ejecutar

	//mandarle a la umv el cambio de proceso activo  pcb->id
	//con el indice de codigo y el pc obtengo la posicion de la proxima instruccion a ejecutar
	u_int32_t proxInstBase = pcb->segmento_codigo;
	u_int32_t proxInstOffset = (pcb->indice_codigo)+ 8*(1-pcb->program_counter);
	u_int32_t proxInstTamanio = proxInstOffset + 4;
	memcpy(pedido,&id_mensaje,sizeof(id_mensaje));
	memcpy(pedido+sizeof(id_mensaje),&proxInstBase,sizeof(proxInstBase));
	memcpy(pedido+sizeof(id_mensaje)+sizeof(proxInstBase), &proxInstOffset, sizeof(proxInstOffset));
	memcpy(pedido+sizeof(id_mensaje)+sizeof(proxInstBase)+sizeof(proxInstOffset), &proxInstTamanio, sizeof(proxInstTamanio));

	//le mando a la umv base, offset y tamanio de la proxima instruccion
	send(socket,pedido,sizeof(u_int32_t)*4,0);

	//recibir la instruccion a ejecutar
	char* instruccion = malloc(proxInstTamanio);
	recv(socket,instruccion,sizeof(instruccion),0);
	printf("Instruccion que voy a ejecutar %s\n",instruccion);
	return instruccion;
}

void signal_handler(int sig){
	if(sig == SIGUSR1){
	printf("Le llego la señal sigusr");
	//desconectarse luego de la ejecucion actual dejando de dar servicio al sistema
	//mandarle el pcb al pcp
	//cerrar conexiones
	//CONEC_CERRADA_SIGUSR1 para que me borren del diccionario
	//liberar memoria
	}
}

void recibirUnPcb(){
	msj = socket_recv_quantum_pcb(socketKernel);
	recv_pcb_del_kernel(msj);
}


void recv_pcb_del_kernel(t_men_quantum_pcb *men){
 pcb = men->pcb;
 quantum = men->quantum;
}


 void handshake_umv(){
	 t_men_comun *men_hs;
	 men_hs = socket_recv_comun(socketUmv);
	 if(men_hs->tipo != HS_UMV)
		 printf("ERROR se esperaba HS_UMV y se recibio %i\n",men_hs->tipo);
		 men_hs->tipo = HS_CPU;
		 socket_send_comun(socketKernel, men_hs);

	 /*t_men_comun *handshake = crear_men_comun(HS_CPU,"",1);//lo mismo que en handshake kernel
	socket_send_serealizado(socketUmv,handshake);

	handshake = socket_recv_serealizado(socketUmv);

	if(handshake->tipo == HS_UMV){
		printf("UMV conectada\n");
	}else{
		printf("ERROR HANDSHAKE UMV = %i\n",handshake->tipo);
	}*/
}

void handshake_kernel(){
	 t_men_comun *men_hs;
		 men_hs = socket_recv_comun(socketKernel);
		 if(men_hs->tipo != HS_KERNEL)
		  	printf("ERROR se esperaba HS_KERNEL y se recibio %i\n",men_hs->tipo);
		  men_hs->tipo = HS_CPU;
		  socket_send_comun(socketKernel, men_hs);

	/*t_men_comun *handshake = crear_men_comun(HS_CPU,"",1);// verificar que HS hay que usar antes habia definido un HS_KERNEL_CPU
	socket_send_serealizado(socketKernel,handshake);

	handshake = socket_recv_serealizado(socketKernel);
	if(handshake->tipo == HS_KERNEL){
		printf("KERNEL conectada\n");
	}else{
		printf("ERROR HANDSHAKE KERNEL = %i\n",handshake->tipo);
	}*/
}


//Primitivas ANSISOP


t_puntero definirVariable(t_nombre_variable identificador_variable){
	// almacenar en umv base=stack  offset= tamaño= buffer=
	//solicitar a la umv
	//recibir de la umv un puntero a la nueva variable = puntNuevo
	//dictionary_put(dicVariables, identificador_variable, puntNuevo)
	//pcb->tamanio_context ++;
	//return puntNuevo
	printf("definir la variable %c\n",identificador_variable);
	return POSICION_MEMORIA;
}


t_puntero obtenerPosicionVariable(t_nombre_variable identificador_variable){
	printf("Obtener posicion de %c\n", identificador_variable);
	return POSICION_MEMORIA;
	//return dictionary_get(diccionario, identificador de variable)
	/*t_puntero posicionVariable = *dictionary_get(dicVariables, identificador_variable);
	// si no esta, devolver -1
	if (posicionVariable == NULL){
		posicionVariable = -1;
	}
	printf("La posicion es %s\n", posicionVariable);
	return posicionVariable;
*/
}



t_valor_variable dereferenciar(t_puntero direccion_variable){
	//obtiene los cuatro bytes siguientes a direccion_variable
	//solictar a umv base=contexto_actual  offset=direccion_variable tamaño=4 bytes
	//recibir de umv 4 bytes de valor_variable
	// return valor_variable
	printf("Dereferenciar %d y su valor es: %d\n",direccion_variable, CONTENIDO_VARIABLE);
	return CONTENIDO_VARIABLE;
}


void asignar(t_puntero direccion_variable, t_valor_variable valor){
	//envio a la umv para almacenar base= contexto actual offset= direccion_variable tamaño=4bytes buffer= valor
	//cambiar va
	printf("Asignando en el valor \n");
}



t_valor_variable obtenerValorCompartida(t_nombre_compartida variable){
	//le mando al kernel el nombre de la variable compartida
	//recibo el valor
	printf("Obteniendo el valor de compartida %s que es %c", variable, CONTENIDO_VARIABLE);
	return CONTENIDO_VARIABLE;
}

t_valor_variable asignarValorCompartida(t_nombre_compartida variable, t_valor_variable valor){
	//le mando al kernel el nombre de la variable compartida
	//le mando el valor que le quiero asignar
	//recibir el valor que le asigne
	printf("Asignando a variable compartida %s el valor %c \n", variable, CONTENIDO_VARIABLE);
	return CONTENIDO_VARIABLE;
	}


void irAlLabel(t_nombre_etiqueta t_nombre_etiqueta){
	// primer instruccion ejecutable de etiqueta y -1 en caso de error
	/* Cambia la linea de ejecucion a la correspondiente de la etiqueta buscada.
			 * @sintax	TEXT_GOTO (goto )
			 * @param	t_nombre_etiqueta	Nombre de la etiqueta
			 * @return	void
			 */
	printf("Yendo al label \n");

}

void llamarSinRetorno(t_nombre_etiqueta etiqueta){
	/*Preserva el contexto de ejecución actual para poder retornar luego al mismo.
			 * Modifica las estructuras correspondientes para mostrar un nuevo contexto vacío.
			 *
			 * Los parámetros serán definidos luego de esta instrucción de la misma manera que una variable local, con identificadores numéricos empezando por el 0.
			 *
			 * @sintax	Sin sintaxis particular, se invoca cuando no coresponde a ninguna de las otras reglas sintacticas
			 * @param	etiqueta	Nombre de la funcion
			 * @return	void
			 */
	printf("Llamando sin retorno a etiqueta\n");
	}

void llamarConRetorno(t_nombre_etiqueta etiqueta, t_puntero donde_retornar){
	/* Preserva el contexto de ejecución actual para poder retornar luego al mismo, junto con la posicion de la variable entregada por donde_retornar.
		 * Modifica las estructuras correspondientes para mostrar un nuevo contexto vacío.
		 *
		 * Los parámetros serán definidos luego de esta instrucción de la misma manera que una variable local, con identificadores numéricos empezando por el 0.
		 *
		 * @sintax	TEXT_CALL (<-)
		 * @param	etiqueta	Nombre de la funcion
		 * @param	donde_retornar	Posicion donde insertar el valor de retorno
		 * @return	void
		 */
	printf("Llamando con retorno a etiqueta");
}

void finalizar(void){
	/*
			 * FINALIZAR
			 *
			 * Cambia el Contexto de Ejecución Actual para volver al Contexto anterior al que se está ejecutando, recuperando el Cursor de Contexto Actual y el Program Counter previamente apilados en el Stack.
			 * En caso de estar finalizando el Contexto principal (el ubicado al inicio del Stack), deberá finalizar la ejecución del programa.
			 *
			 * @sintax	TEXT_END (end )
			 * @param	void
			 * @return	void
			 */
	printf("Finalizando");
}

void retornar(t_valor_variable retorno){
	/*
			 * RETORNAR
			 *
			 * Cambia el Contexto de Ejecución Actual para volver al Contexto anterior al que se está ejecutando, recuperando el Cursor de Contexto Actual, el Program Counter y la direccion donde retornar, asignando el valor de retorno en esta, previamente apilados en el Stack.
			 *
			 * @sintax	TEXT_RETURN (return )
			 * @param	retorno	Valor a ingresar en la posicion corespondiente
			 * @return	void
			 */
	printf("Retornando valor de variable%d\n", retorno);
}

void imprimir(t_valor_variable valor_mostrar){
	char *aux = string_itoa(valor_mostrar);
	t_men_comun *men = crear_men_comun(IMPRIMIR_VALOR,aux ,string_length(aux));
	socket_send_serealizado(socketKernel,men);
	men = crear_men_comun(ID_PROG,string_itoa(pcb->id) ,string_length(string_itoa(pcb->id)));
	socket_send_serealizado(socketKernel,men);
	destruir_t_mensaje(men);
	printf("Imprimiendo valor de variable %d\n", valor_mostrar);
}


void imprimirTexto(char* texto){
	t_men_comun *men = crear_men_comun(IMPRIMIR_TEXTO, texto,string_length(texto));
	socket_send_serealizado(socketKernel,men);
	men = crear_men_comun(ID_PROG,string_itoa(pcb->id) ,string_length(string_itoa(pcb->id)));
	socket_send_serealizado(socketKernel,men);
	destruir_t_mensaje(men);
	printf("Imprimiendo texto: %s", texto);
	}

void entradaSalida(t_nombre_dispositivo dispositivo, int tiempo){
	t_men_comun *men = crear_men_comun(IO_ID, dispositivo,string_length(dispositivo));
	char *cant_unidades = string_itoa(tiempo);
	men = crear_men_comun(IO_CANT_UNIDADES, cant_unidades,string_length(cant_unidades));
	socket_send_serealizado(socketKernel,men);
	destruir_t_mensaje(men);
	printf("Saliendo a entrada/salida en dispositivo por esta cantidad de tiempo\n");
}

void wait(t_nombre_semaforo identificador_semaforo){
	t_men_comun *men = crear_men_comun(WAIT, identificador_semaforo,string_length(identificador_semaforo));
	socket_send_serealizado(socketKernel,men);
	destruir_t_mensaje(men);
	printf("Haciendo wait a semaforo%s\n", identificador_semaforo);
}
void mi_signal(t_nombre_semaforo identificador_semaforo){
	t_men_comun *men = crear_men_comun(SIGNAL, identificador_semaforo,string_length(identificador_semaforo));
	socket_send_serealizado(socketKernel,men);
	destruir_t_mensaje(men);
	printf("Haciendo signal a semaforo%s\n", identificador_semaforo);

}



