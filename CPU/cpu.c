/*
 * cpu.c
 *
 *  Created on: 01/06/2014
 *      Author: utnso
 */

#include "cpu.h"



int quantum;
int quit_sistema = 1;
int socketKernel;
int socketUmv;
char *prox_inst;
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
	t_pcb *pcb = malloc(sizeof(t_pcb));
	got_usr1 = 1;

	sa.sa_handler = signal_handler; // puntero a una funcion handler del signal
	sa.sa_flags = 0; // flags especiales para afectar el comportamiento de la señal
	sigemptyset(&sa.sa_mask); // conjunto de señales a bloquear durante la ejecucion del signal-catching function


	t_config *unaConfig = config_create("config.conf");
	char *ipKernel=config_get_string_value(unaConfig, "IPKERNEL");
	char *puertoKernel = config_get_string_value(unaConfig, "PUERTOKERNEL");
	char *puertoUmv = config_get_string_value(unaConfig, "PUERTOUMV");
	char *ipUmv=config_get_string_value(unaConfig, "IPKERNEL");


	/*conexion con el kernel*/

	handshake_kernel(puertoKernel, ipKernel);

	/*conexion con la umv*/
	handshake_umv(puertoUmv, ipUmv);


	while(1){

		/*recibe un pcb del kernel*/
		recibirUnPcb();

		/*se crea un diccionario para guardar las variables del contexto*/
		crearDiccionario();

		/*maneja si recibe la señal SIGUSR, hay que revisarlo todavia*/
		if(sigaction(SIGUSR1, &sa, NULL) == -1){// sigaction permite llamar o especificar la accion asociada a la señal
			perror(sigaction);
			exit(1);
		}

		int32_t cantIns = 0;

		char* proxInstrucc = solicitarProxSentenciaAUmv();

		/*if (strcmp(proxInstrucc, "error") ){
				errorDeProxInstruccion(socketKernel); // le aviso al pcp que la umv me devolvio error al solicitar prox inst
				}else{*/
					pcb->program_counter ++; // aumento el pc
					if (pcb->program_counter == pcb->cant_instrucciones){ // me fijo si la sentencia a ejecutar es la ultima del script
						parsearUltimaInstruccion(proxInstrucc, socketKernel);}
						else{
							for(;quantum>0;quantum--){
								//ejecuta tantas instrucciones como quantums tenga
								parsearUnaInstruccion(proxInstrucc);
								cantIns ++;
							}
							salirPorQuantum();


					}

				}

	}
	//}

	//recv(socketKernel,pcb, sizeof(estructura_pcb),0);	recv(socketKernel, &quantum, sizeof(int), 0);

	//crear diccionario de variables


socket_cerrar(socketKernel);
socket_cerrar(socketUmv);

dictionary_destroy(dic_Variables);

return 0;

}



void crearDiccionario(){
	dic_Variables = dictionary_create();

	//si se trata de un programa con un  stack que ya tiene variables
	int32_t tam_contexto = pcb->cant_var_contexto_actual;
	int32_t i = 1;
	if (tam_contexto > 0){//regenerar diccionario
		while(i<=tam_contexto){
			base = pcb->dir_primer_byte_umv_contexto_actual;
			offset= (i-1)*5;
			tam=1;

			t_men_cpu_umv *sol_var = crear_men_cpu_umv(SOL_BYTES, base, offset, tam, NULL);
			socket_send_cpu_umv(socketUmv, sol_var);
			destruir_men_cpu_umv(sol_var);

			t_men_comun *rec_var = socket_recv_comun(socketUmv);
			if(rec_var->tipo == CONEC_CERRADA){//manejar

			}
			char *key = malloc(rec_var->tam_dato);
			key = rec_var->dato;
			char *data = malloc(sizeof(string_itoa(base + offset)));
			data = string_itoa(base + offset);

			dictionary_put(dic_Variables, key, data);

			destruir_men_comun(rec_var);
			free(data);
			free(key);


			i++;



		}

		}


	}





void salirPorQuantum(){
	//mando el pcb con un tipo de mensaje que sali por quantum
	socket_send_quantum_pcb(socketKernel, crear_men_quantum_pcb(FIN_QUANTUM,0, pcb));


}
void parsearUltimaInstruccion(char* ultIns, int socketKernel){
	analizadorLinea(strdup(ultIns), &functions, &kernel_functions);
	//avisar al pcp que termino el programa
	socket_send_quantum_pcb(socketKernel, crear_men_quantum_pcb(FIN_EJECUCION,0, pcb));

}

void parsearUnaInstruccion(char* unaIns){
	analizadorLinea(strdup(unaIns), &functions, &kernel_functions);
}

/*void errorDeProxInstruccion(int socketKernel){
	//avisar al pcp que hubo un error de en el pedido de la instruccion
	u_int32_t codigoErrorEnUmv = 3;
	avisarAlPcp(codigoErrorEnUmv, socketKernel);
	//mostrar por pantalla que hubo un excepcion
	printf("Ocurrio una excepcion en el pedido a la UMV\n");
	//concluir la ejecucion de un programa

}*/

char* solicitarProxSentenciaAUmv(){
	char* proxInst = NULL;

	//mandarle a la umv el cambio de proceso activo  pcb->id
	//con el indice de codigo y el pc obtengo la posicion de la proxima instruccion a ejecutar
	int32_t base = pcb->dir_primer_byte_umv_segmento_codigo;
	int32_t offset = (pcb->dir_primer_byte_umv_indice_codigo)+ 8*(1-pcb->program_counter);
	int32_t tam = sizeof(uint32_t)*4;

	//le mando a la umv base, offset y tamanio de la proxima instruccion
	t_men_cpu_umv *sol_inst = crear_men_cpu_umv(SOL_BYTES, base, offset, tam, NULL);
	socket_send_cpu_umv(socketUmv, sol_inst);
	destruir_men_cpu_umv(sol_inst);

	//recibir la instruccion a ejecutar
	t_men_comun *rec_inst = socket_recv_comun(socketUmv);

	if(rec_inst->tipo == PROX_INSTRUCCION){
		char* proxInst = rec_inst->dato;
		printf("Instruccion que voy a ejecutar %s\n",proxInst);
	}
	if(rec_inst->tipo == CONEC_CERRADA){
		printf("Se cerro la conexion de la umv\n");
	}

	return proxInst;
}

void signal_handler(int sig){
	got_usr1 =1;
	t_men_quantum_pcb *m = crear_men_quantum_pcb(CPU_DESCONEC, 0, pcb);
	socket_send_quantum_pcb(socketKernel, m);
	destruir_quantum_pcb(m);

	//t_men_cpu_umv *c = crear_men_cpu_umv(CAMBIO_PA, )
	/*if(sig == SIGUSR1){
	pid_t pid= getpid();
	kill(pid, SIGUSR1);
	printf("Le llego la señal sigusr"); // no es async safe
	exit(1);
	//desconectarse luego de la ejecucion actual dejando de dar servicio al sistema
	//mandarle el pcb al pcp
	//cerrar conexiones
	//CONEC_CERRADA_SIGUSR1 para que me borren del diccionario
	//liberar memoria*/
	}
}

void recibirUnPcb(){
	t_men_quantum_pcb *m = socket_recv_quantum_pcb(socketKernel);
	if(m->tipo==CONEC_CERRADA){
		//avisar al pcp
	}else {
		pcb = m->pcb;
		quantum = m->quantum;
	}

}


void handshake_umv(char *ip_umv, char *puerto_umv){
	//envio a la UMV
	socketUmv = socket_crear_client(puerto_umv,ip_umv);
	t_men_comun *mensaje_inicial = crear_men_comun(HS_CPU,"",string_length(""));
	socket_send_comun(socketUmv,mensaje_inicial);
	//espero coneccion de la UMV
	mensaje_inicial = socket_recv_comun(socketUmv);
	if(mensaje_inicial->tipo == HS_UMV){
		printf("UMV conectada\n");
	}else{
		printf("ERROR HANDSHAKE");
	}
}


void handshake_kernel(char *ip_k, char *puerto_k){
	//envio al kernel
	socketKernel = socket_crear_client(puerto_k,ip_k);
	t_men_comun *mensaje_inicial = crear_men_comun(HS_CPU,"",string_length(""));
	socket_send_comun(socketKernel,mensaje_inicial);
	//espero conexion de kernel
	mensaje_inicial = socket_recv_comun(socketKernel);
	if(mensaje_inicial->tipo == HS_KERNEL){
		printf("Kernel conectado\n");
	}else{
		printf("ERROR HANDSHAKE");
	}
}


void preservarContexto(){
	// guardar en stack ptro contexto actual
		base = pcb->dir_primer_byte_umv_contexto_actual;
		offset = base + (pcb->cant_var_contexto_actual)+1;	// la posicion siguiente al ultimo valor de la ultima varaible
		tam = sizeof(int32_t);
		char * buffer = malloc(tam);
		buffer = string_itoa(base);

		t_men_cpu_umv *save_context = crear_men_cpu_umv(ALM_BYTES,base, offset, tam, buffer);
		socket_send_cpu_umv(socketUmv, save_context);
		destruir_men_cpu_umv(save_context);
		//controlar stack overflow

		//program counter + 1 (siguiente instruccion a ejecutar)
		offset = offset + 4;
		buffer = string_itoa(pcb->program_counter);
		t_men_cpu_umv *save_proxins = crear_men_cpu_umv(ALM_BYTES, base, offset, tam, buffer);
		send_men_cpu_umv(socketUmv, save_proxins);
		destruir_men_cpu_umv(save_proxins);

		free(buffer);

}

void finalizarContexto(){
	t_men_quantum_pcb *fin_ej= crear_men_quantum_pcb(FIN_EJECUCION,0, pcb);
	socket_send_quantum_pcb(socketKernel, fin_ej);
	destruir_men_quantum_pcb(fin_ej);
}
//Primitivas ANSISOP


t_puntero definirVariable(t_nombre_variable identificador_variable){

	base = pcb->dir_primer_byte_umv_contexto_actual;
	offset= 1+ (pcb->cant_var_contexto_actual)*5 ;
	int32_t tam = sizeof(int32_t)+1;
	char* buffer = malloc(1);
	buffer = string_itoa(identificador_variable);
	t_men_cpu_umv *alm_bytes = crear_men_cpu_umv(ALM_BYTES, base, offset,tam , buffer);
	send_men_cpu_umv(socketUmv, alm_bytes);
	destruir_men_cpu_umv(alm_bytes);
	free(buffer);


	dictionary_put(*dic_Variables, identificador_variable, itoa(offset));
	(pcb->cant_var_contexto_actual) ++;

	printf("Definir la variable %c en la posicion %c\n",identificador_variable, offset);
	return offset;
}


t_puntero obtenerPosicionVariable(t_nombre_variable identificador_variable){
	/*
	 * Se fija en el diccionario de variables la posicion
	 * la posicion va a ser el data del elemento
	 *
	 * no me funciona
	 * */
	t_puntero posicion ;
	char* data = *dictionary_get(*dic_Variables, t_nombre_variable);
	if (data == NULL){
		posicion = -1;
		}else {
			posicion= string_atoi(data);
		}

	printf("La posicion de la variable %s es %d", identificador_variable, posicion);

	return posicion ;



}



t_valor_variable dereferenciar(t_puntero direccion_variable){
	//obtiene los cuatro bytes siguientes a direccion_variable

	t_valor_variable valor;

	int32_t base = pcb->dir_primer_byte_umv_contexto_actual;
	int32_t offset = 1 + direccion_variable; // son los cuatro bytes siguientes a la variable
	int32_t tam= sizeof(int32_t);

	t_men_cpu_umv *sol_val = *crear_men_cpu_umv(SOL_BYTES,base, offset, tam, NULL);

	send_men_cpu_umv(socketUmv, *sol_val);

	//recibir de umv 4 bytes de valor_variable

	*men_comun = socket_recv_comun(socketUmv);

	if (men_comun->tipo == R_SOL_BYTES){

		valor = atoi(men_comun->dato);
	}

	printf("Dereferenciar %d y su valor es: %d\n",direccion_variable,valor);

	return valor;
}


void asignar(t_puntero direccion_variable, t_valor_variable valor){
	//envio a la umv para almacenar base= contexto actual offset= direccion_variable tamaño=4bytes buffer= valor
	int32_t base = pcb->dir_primer_byte_umv_contexto_actual;
	int32_t offset = 1 + direccion_variable;
	int32_t tam = sizeof(t_valor_variable);
	char* buffer = malloc(tam);
	buffer = string_itoa(valor);
	t_men_cpu_umv  *men_asig= crear_men_cpu_umv(ALM_BYTES, base, offset, tam, buffer);
	send_men_cpu_umv(socketUmv, men_asig);
	destruir_men_cpu_umv(men_asig);

	//cambiar va
	printf("Asignando en la direccion %d el valor %d \n", direccion_variable, valor);
}



t_valor_variable obtenerValorCompartida(t_nombre_compartida variable){
	//le mando al kernel el nombre de la variable compartida
	t_men_comun *m = crear_men_comun(OBTENER_VALOR, variable, sizeof(variable));
	socket_send_comun(socketKernel, m);
	//recibo el valor
	t_men_comun *respuesta = socket_recv_comun(socketKernel);
	char *c = respuesta->dato;
	t_valor_variable valor = atoi(c);
	printf("Obteniendo el valor de compartida %s que es %c", variable, valor);
	return valor;
}

t_valor_variable asignarValorCompartida(t_nombre_compartida variable, t_valor_variable valor){
	//le mando al kernel el nombre de la variable compartida
	//le mando el valor que le quiero asignar
	t_men_comun *var =crear_men_comun(GRABAR_VALOR, variable, sizeof(variable));
	char *c =  string_itoa(valor);
	t_men_comun *val_asignado = crear_men_comun(VALOR_ASIGNADO, c, sizeof(c));
	socket_send_comun(socketKernel,var);
	socket_send_comun(socketKernel, val_asignado);

	//recibir el valor que le asigne
	t_men_comun *ms = socket_recv_comun(socketKernel);
	//char *dato = ms->dato;
	t_valor_variable val = atoi(ms->dato);
	printf("Asignando a variable compartida %s el valor %d \n", variable, val);
	return val;
	}


void irAlLabel(t_nombre_etiqueta t_nombre_etiqueta){

	// primer instruccion ejecutable de etiqueta y -1 en caso de error
	base = pcb->dir_primer_byte_umv_indice_etiquetas

	t_puntero_instruccion *etiqueta= metadata_buscar_etiqueta(t_nombre_etiqueta, pcb->dir_primer_byte_umv_indice_etiquetas, pcb->tam_indice_etiquetas);
	offset= etiqueta;
	t_men_comun *m = crear_men_cpu_umv(SOL_BYTES, base, offset, tam, NULL);
	send_men_cpu_umv(socketUmv, m);
	destruir_men_cpu_umv(m);

	// faltaria que hacer con la posicion de la etiqueta, no estoy segura que esta bien

	/* Cambia la linea de ejecucion a la correspondiente de la etiqueta buscada.
			 * @sintax	TEXT_GOTO (goto )
			 * @param	t_nombre_etiqueta	Nombre de la etiqueta
			 * @return	void
			 */
	printf("Yendo al label %s\n", t_nombre_etiqueta);

}

void llamarSinRetorno(t_nombre_etiqueta etiqueta){
	/*Preserva el contexto de ejecución actual para poder retornar luego al mismo.*/

	preservarContexto();


	pcb->cant_var_contexto_actual = 0;
	pcb->dir_primer_byte_umv_contexto_actual = (pcb->dir_primer_byte_umv_contexto_actual)+5*(pcb->cant_var_contexto_actual)+1;

	irAlLabel(etiqueta);

	/* Modifica las estructuras correspondientes para mostrar un nuevo contexto vacío.
	* Los parámetros serán definidos luego de esta instrucción de la misma manera que una variable local, con identificadores numéricos empezando por el 0.
    enviar a la umv el valor del cursor del contexto actual  y
    progrma counter aumentado en uno
    actualiza el cursor del contexto para que apunte al final de los valores que mande a guardar a la umv
    actualiza el valor del program counter con lo que me devuelve el irallabel


    */


	printf("Llamando sin retorno a etiqueta\n");
	}



void llamarConRetorno(t_nombre_etiqueta etiqueta, t_puntero donde_retornar){
	//preservar el contexto

	preservarContexto();


	// dir de retorno

	offset = offset + 4;
	char* buffer = malloc(sizeof(int32_t));
	buffer = string_itoa(donde_retornar);
	t_men_cpu_umv *save_retorno = crear_men_cpu_umv(ALM_BYTES, base, offset, tam, buffer);
	send_men_cpu_umv(socketUmv, save_retorno);
	destruir_men_cpu_umv(save_retorno);

	free(buffer);

	pcb->cant_var_contexto_actual = 0;
	pcb->dir_primer_byte_umv_contexto_actual = (pcb->dir_primer_byte_umv_contexto_actual)+5*(pcb->cant_var_contexto_actual)+1;
	irAlLabel(etiqueta);


	printf("Llamando con retorno a etiqueta %s y retorna en %d", etiqueta, donde_retornar);
}

void finalizar(void){
	base=pcb->dir_primer_byte_umv_contexto_actual;
	offset= base -4;
	tam=sizeof(int32_t);

	t_men_cpu_umv *sol_progcount = crear_men_cpu_umv(SOL_BYTES, base, offset, tam, NULL);
	send_men_cpu_umv(socketUmv, sol_progcount);
	destruir_men_cpu_umv(sol_progcount);

	t_men_comun *rec_progcount= socket_recv_comun(socketUmv);
	int32_t prog_counter = atoi(rec_progcount->dato);
	pcb->program_counter= prog_counter;

	offset= offset-4;

	t_men_cpu_umv *sol_context = crear_men_cpu_umv(SOL_BYTES, base, offset, tam, NULL);
	send_men_cpu_umv(socketUmv, sol_context);
	destruir_men_cpu_umv(sol_context);

	t_men_comun *rec_context= socket_recv_comun(socketUmv);
	int32_t context = atoi(rec_context->dato);
	pcb->dir_primer_byte_umv_contexto_actual = context;

	if(pcb->dir_primer_byte_umv_contexto_actual == pcb->dir_primer_byte_umv_segmento_stack){

	finalizarContexto();

	}

	printf("Finalizando el programa actual");
}

void retornar(t_valor_variable retorno){

	base=pcb->dir_primer_byte_umv_contexto_actual;
	offset= base -4;
	tam=sizeof(int32_t);

	t_men_cpu_umv *sol_ret = crear_men_cpu_umv(SOL_BYTES,base, offset, tam, NULL);
	send_men_cpu_umv(socketUmv, sol_ret);
	destruir_men_cpu_umv(sol_ret);

	t_men_comun *rec_ret= socket_recv_comun(socketUmv);
	int32_t dir_retorno = atoi(rec_ret->dato);


	offset= offset-4;

	t_men_cpu_umv *sol_progcount = crear_men_cpu_umv(SOL_BYTES, base, offset, tam, NULL);
	send_men_cpu_umv(socketUmv, sol_progcount);
	destruir_men_cpu_umv(sol_progcount);

	t_men_comun *rec_progcount= socket_recv_comun(socketUmv);
	int32_t prog_counter = atoi(rec_progcount->dato);
	pcb->program_counter= prog_counter;

	offset= offset-4;

	t_men_cpu_umv *sol_context = crear_men_cpu_umv(SOL_BYTES, base, offset, tam, NULL);
	send_men_cpu_umv(socketUmv, sol_context);
	destruir_men_cpu_umv(sol_context);

	t_men_comun *rec_context= socket_recv_comun(socketUmv);
	int32_t context = atoi(rec_context->dato);
	pcb->dir_primer_byte_umv_contexto_actual = context;

	base = context;
	offset= dir_retorno +1;
	char* buffer = string_itoa(retorno);
	t_men_cpu_umv *alm_val = crear_men_cpu_umv(ALM_BYTES, base, offset, tam, buffer);
	send_men_cpu_umv(socketUmv, alm_val);
	destruir_men_cpu_umv(alm_val);

	printf("Retornando valor de variable%d en la direccion %d\n", retorno, dir_retorno);
}

void imprimir(t_valor_variable valor_mostrar){
	char *aux = string_itoa(valor_mostrar);
	t_men_comun *men = crear_men_comun(IMPRIMIR_VALOR,aux ,string_length(aux));
	socket_send_comun(socketKernel,men);

	men = crear_men_comun(ID_PROG,string_itoa(pcb->id) ,string_length(string_itoa(pcb->id)));
	socket_send_comun(socketKernel,men);
	destruir_men_comun(men);

	printf("Imprimiendo valor de variable %d\n", valor_mostrar);
}


void imprimirTexto(char* texto){
	t_men_comun *men = crear_men_comun(IMPRIMIR_TEXTO, texto,string_length(texto));
	socket_send_comun(socketKernel,men);

	men = crear_men_comun(ID_PROG,string_itoa(pcb->id) ,string_length(string_itoa(pcb->id)));
	socket_send_comun(socketKernel,men);
	destruir_men_comun(men);

	printf("Imprimiendo texto: %s", texto);
	}

void entradaSalida(t_nombre_dispositivo dispositivo, int tiempo){
	t_men_comun *men = crear_men_comun(IO_ID, dispositivo,string_length(dispositivo));
	char *cant_unidades = string_itoa(tiempo);
	men = crear_men_comun(IO_CANT_UNIDADES, cant_unidades,string_length(cant_unidades));
	socket_send_comun(socketKernel,men);
	destruir_men_comun(men);


	t_men_quantum_pcb *mpcb = crear_men_quantum_pcb(PCB_Y_QUANTUM, 0, pcb);
	socket_send_quantum_pcb(socketKernel, mpcb);
	destruir_men_quantum_pcb(mpcb);

	printf("Saliendo a entrada/salida en dispositivo %s por esta cantidad de tiempo%d\n", dispositivo, tiempo);
}

void wait(t_nombre_semaforo identificador_semaforo){
	t_men_comun *men = crear_men_comun(WAIT, identificador_semaforo,string_length(identificador_semaforo));
	socket_send_comun(socketKernel,men);
	destruir_men_comun(men);

	printf("Haciendo wait a semaforo%s\n", identificador_semaforo);
}
void mi_signal(t_nombre_semaforo identificador_semaforo){
	t_men_comun *men = crear_men_comun(SIGNAL, identificador_semaforo,string_length(identificador_semaforo));
	socket_send_comun(socketKernel,men);
	destruir_men_comun(men);

	printf("Haciendo signal a semaforo%s\n", identificador_semaforo);

}



