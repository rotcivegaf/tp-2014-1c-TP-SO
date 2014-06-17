/*
 * cpu.c
 *
 *  Created on: 01/06/2014
 *      Author: utnso
 */

#include "cpu.h"

static const int CONTENIDO_VARIABLE = 20;
static const int POSICION_MEMORIA = 0x10;


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

	/*levanta archivo de configuracion para obtener ip y puerto de kernel y umv*/
	t_datos_config *unaConfig = *levantarConfiguracion();



	/*conexion con el kernel*/

	int socketKernel = conectarse(unaConfig->ipKernel,unaConfig->puertoKernel);

	//queda a la espera que el pcp  envie el pcb
	//recibe el pcb
	estructura_pcb *pcb = malloc(sizeof(estructura_pcb));
	int quantum;
	recv(socketKernel,pcb, sizeof(estructura_pcb),0);
	recv(socketKernel, &quantum, sizeof(int), 0);


	//crear diccionario de variables
	crearDiccionario();

	signal(SIGUSR1, signal_handler);

	int cantIns = 0;

	/*conexion con la umv*/
	int socketUmv = conectarse(unaConfig->ipUmv,unaConfig->puertoUmv);

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


/*close(socketKernel);
close(socketUmv);*/

destruirDiccionario();

return 0;

}

t_datos_config *levantarConfiguracion(){
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
}

void crearDiccionario(){
	t_dictionary dicVariables = *dictionary_create();
	//si el tamaño de contexto es distinto de cero
		// for (de tamaño de contexto a 0){
			//pedir a umv, base= stack offset=(i-1)x5 tamanio=1
			//recibir de la umv key
			//dictionary_put(dicVariables,key, stack + (i-1))
	//}
}

void destruirDiccionario(){
	dictionary_remove_and_destroy(dicVariables);
}

void avisarAlPcp(int unCodigo, int socketKernel){
	char *pedido = malloc(sizeof(int));
	int id_mensaje = 6;
	memcpy(pedido, &id_mensaje, sizeof(id_mensaje));
	send(socketKernel,pedido,sizeof(int),0);
}

void salirPorQuantum(int socketKernel, estructura_pcb *pcb){
	//aviso al pcp que termino mi quantum
	int codigoQuantum =1;
	avisarAlPcp(codigoQuantum, socketKernel);
	//le mando el pcb actualizado;
	send(socketKernel,pcb, sizeof(estructura_pcb),0);

}
void parsearUltimaInstruccion(char* ultIns, int socketKernel){
	analizadorLinea(strdup(ultIns), &functions, &kernel_functions);
	//avisar al pcp que termino el programa
	int codigoFinProg = 2;
	avisarAlPcp(codigoFinProg, socketKernel);
}

void parsearUnaInstruccion(char* unaIns){
	analizadorLinea(strdup(unaIns), &functions, &kernel_functions);
}

void errorDeProxInstruccion(int socketKernel){
	//avisar al pcp que hubo un error de en el pedido de la instruccion
	int codigoErrorEnUmv = 3;
	avisarAlPcp(codigoErrorEnUmv, socketKernel);
	//mostrar por pantalla que hubo un excepcion
	printf("Ocurrio una excepcion en el pedido a la UMV\n");
	//concluir la ejecucion de un programa

}

char* solicitarProxSentenciaAUmv(int socket, estructura_pcb *pcb){
	//para decir que se esta conectando una conexion tipo cpu
	char *pedido = malloc(sizeof(int));
	int id_mensaje = 4; //para saber que le estoy pidiendo prox instruccion a ejecutar

	//mandarle a la umv el cambio de proceso activo  pcb->id
	//con el indice de codigo y el pc obtengo la posicion de la proxima instruccion a ejecutar
	int proxInstBase = pcb->segmento_codigo;
	int proxInstOffset = (pcb->indice_codigo)+ 8*(1-pcb->program_counter);
	int proxInstTamanio = proxInstOffset + 4;
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
	//return dictionary_get(diccionario, identificador de variable)
	t_puntero posicionVariable = *dictionary_get(dicVariables, identificador_variable);
	// si no esta, devolver -1
	if (posicionVariable == NULL){
		posicionVariable = -1;
	}
	printf("La posicion es %s\n", posicionVariable);
	return posicionVariable;

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
	printf("Obteniendo el valor de compartida %s que es %s", variable, CONTENIDO_VARIABLE);
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
	printf("Yendo al label \n");

}

void llamarSinRetorno(t_nombre_etiqueta etiqueta){
	printf("Llamando sin retorno a etiqueta\n");
	}

void llamarConRetorno(t_nombre_etiqueta etiqueta, t_puntero donde_retornar){
	printf("Llamando con retorno a etiqueta");
}

void finalizar(void){
	printf("Finalizando");
}

void retornar(t_valor_variable retorno){
	printf("Retornando valor de variable%d\n", retorno);
}

void imprimir(t_valor_variable valor_mostrar){
	/*char *aux = string_itoa(valor_mostrar);
	t_mensaje *men = crear_t_mensaje(IMPRIMIR_VALOR,aux ,string_length(aux));
	socket_send_serealizado(soc_kernel,men);
	men = crear_t_mensaje(ID_PROG,string_itoa(pcb->id) ,string_length(string_itoa(pcb->id)));
	socket_send_serealizado(soc_kernel,men);
	destruir_t_mensaje(men);*/
	printf("Imprimiendo valor de variable %d\n", valor_mostrar);
}


void imprimirTexto(char* texto){
	/*t_mensaje *men = crear_t_mensaje(IMPRIMIR_TEXTO, texto,string_length(texto));
	socket_send_serealizado(soc_kernel,men);
	men = crear_t_mensaje(ID_PROG,string_itoa(pcb->id) ,string_length(string_itoa(pcb->id)));
	socket_send_serealizado(soc_kernel,men);
	destruir_t_mensaje(men);*/
	printf("Imprimiendo texto: %s", texto);
	}

void entradaSalida(t_nombre_dispositivo dispositivo, int tiempo){
	/*t_mensaje *men = crear_t_mensaje(IO_ID, dispositivo,string_length(dispositivo));
	char *cant_unidades = string_itoa(tiempo);
	men = crear_t_mensaje(IO_CANT_UNIDADES, cant_unidades,string_length(cant_unidades));
	socket_send_serealizado(soc_kernel,men);
	destruir_t_mensaje(men);*/
	printf("Saliendo a entrada/salida en dispositivo por esta cantidad de tiempo\n");
}

void wait(t_nombre_semaforo identificador_semaforo){
	/*t_mensaje *men = crear_t_mensaje(WAIT, identificador_semaforo,string_length(identificador_semaforo));
	socket_send_serealizado(soc_kernel,men);
	destruir_t_mensaje(men);*/
	printf("Haciendo wait a semaforo\n");
}
void mi_signal(t_nombre_semaforo identificador_semaforo){
	/*t_mensaje *men = crear_t_mensaje(SIGNAL, identificador_semaforo,string_length(identificador_semaforo));
	socket_send_serealizado(soc_kernel,men);
	destruir_t_mensaje(men);*/
	printf("Haciendo signal a semaforo\n");
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

/*
 * void pedir_umv_prox_instruc(){

	char *dato = string_itoa(pcb->id);
	t_mensaje *men = crear_t_mensaje(PROX_INSTRUCCION,dato,string_length(dato));
	socket_send_serealizado(soc_umv,men);

	dato = string_itoa(pcb->program_counter);
	men = crear_t_mensaje(PROGRAM_COUNTER,dato,string_length(dato));
	socket_send_serealizado(soc_umv,men);
}

void recv_pcb_del_kernel(t_mensaje *men){
	switch(men->tipo){
	case QUANTUM_MAX:
		quantum = atoi(men->dato);
		break;
	case CANT_VAR_CONTEXTO_ACTUAL:
		pcb->cant_var_contexto_actual = atoi(men->dato);
		break;
	case DPBU_CONTEXTO_ACTUAL:
		pcb->dir_primer_byte_umv_contexto_actual = atoi(men->dato);
		break;
	case DPBU_INDICE_CODIGO:
		pcb->dir_primer_byte_umv_indice_codigo = atoi(men->dato);
		break;
	case DPBU_INDICE_ETIQUETAS:
		pcb->dir_primer_byte_umv_indice_etiquetas = atoi(men->dato);
		break;
	case DPBU_SEGMENTO_CODIGO:
		pcb->dir_primer_byte_umv_segmento_codigo = atoi(men->dato);
		break;
	case DPBU_SEGMENTO_STACK:
		pcb->dir_primer_byte_umv_segmento_stack = atoi(men->dato);
		break;
	case ID_PROG:
		pcb->id = atoi(men->dato);
		break;
	case PROGRAM_COUNTER:
		pcb->program_counter = atoi(men->dato);
		break;
	case TAM_INDICE_ETIQUETAS:
		pcb->tam_indice_etiquetas = atoi(men->dato);
		break;
	default:
		printf("El tipo de dato enviado es erroneo\n");
		break;
	}
}
 *
 *
 * void handshake_umv(){
	t_mensaje *handshake = crear_t_mensaje(HS_UMV_CPU,"",1);
	socket_send_serealizado(soc_umv,handshake);

	handshake = socket_recv_serealizado(soc_umv);

	if(handshake->tipo == HS_UMV_CPU){
		printf("UMV conectada\n");
	}else{
		printf("ERROR HANDSHAKE UMV = %i\n",handshake->tipo);
	}
}

void handshake_kernel(){
	t_mensaje *handshake = crear_t_mensaje(HS_KERNEL_CPU,"",1);
	socket_send_serealizado(soc_kernel,handshake);

	handshake = socket_recv_serealizado(soc_kernel);
	if(handshake->tipo == HS_KERNEL_CPU){
		printf("KERNEL conectada\n");
	}else{
		printf("ERROR HANDSHAKE KERNEL = %i\n",handshake->tipo);
	}
}
 *
 * */
