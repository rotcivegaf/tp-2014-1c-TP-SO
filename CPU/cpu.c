#include "cpu.h"
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
		.AnSISOP_entradaSalida 			= entradaSalida};

AnSISOP_kernel kernel_functions = {
		.AnSISOP_wait 	= wait,
		.AnSISOP_signal = mi_signal};

t_dictionary *dic_Variables;
t_pcb *pcb;
int32_t socketKernel, socketUmv;
int32_t quantum_actual;
int32_t quantum_max;
int32_t quit_sistema = 1;
char* etiquetas;
int32_t fueFinEjecucion = 0;
FILE *cpu_file_log;

int main(){
	got_usr1 = 0;
	etiquetas = NULL;

	sa.sa_handler = signal_handler; // puntero a una funcion handler del signal
	sa.sa_flags = 0; // flags especiales para afectar el comportamiento de la señal
	sigemptyset(&sa.sa_mask); // conjunto de señales a bloquear durante la ejecucion del signal-catching function

	cpu_file_log = txt_open_for_append("./CPU/logs/cpu.log");
	txt_write_in_file(cpu_file_log,"---------------------Nueva ejecucion--------------------------------------------------------------------------------------------\n");

	txt_write_in_file(cpu_file_log, "Cargo la configuracion desde el archivo\n");

	t_config *unaConfig = config_create("./CPU/cpu_config");
	char *puertoKernel = config_get_string_value(unaConfig, "Puerto_Kernel");
	char *ipKernel = config_get_string_value(unaConfig, "IP_Kernel");
	char *puertoUmv = config_get_string_value(unaConfig, "Puerto_UMV");
	char *ipUmv = config_get_string_value(unaConfig, "IP_UMV");
	imprimo_config(puertoKernel, ipKernel, puertoUmv, ipUmv);

	/*conexion con el kernel*/
	handshake_kernel(puertoKernel, ipKernel);

	/*conexion con la umv*/
	handshake_umv(puertoUmv, ipUmv);

	/*maneja si recibe la señal SIGUSR, hay que revisarlo todavia*/
	if(sigaction(SIGUSR1, &sa, NULL) == -1){// sigaction permite llamar o especificar la accion asociada a la señal
		perror("sigaction");
		exit(1);
	}

	while(!got_usr1){
		while(quit_sistema){
			fueFinEjecucion = 0;
			/*recibe un pcb del kernel*/
			recibirUnPcb();
			/* le digo a la UMV q cambie el proceso activo */
			cambio_PA(pcb->id);

			traerIndiceEtiquetas();
			/*se crea un diccionario para guardar las variables del contexto*/
			crearDiccionario();

			for (quantum_actual = 0;(quantum_actual<=quantum_max) && (!fueFinEjecucion); quantum_actual++){//aca cicla hasta q el haya terminado los quantums
				char* proxInstrucc = solicitarProxSentenciaAUmv();
				analizadorLinea(proxInstrucc, &functions, &kernel_functions);
				free(proxInstrucc);
			}
			if(!fueFinEjecucion){
				salirPorQuantum();
			}

			cambio_PA(0);//lo cambio a 0 asi la UMV puede comprobar q hay un error
			dictionary_destroy(dic_Variables);
		}
	}

	socket_cerrar(socketKernel);
	socket_cerrar(socketUmv);
	return 0;
}

void cambio_PA(int32_t id_proc){
	printf("Cambio el proceso activo al proceso nº%i\n",id_proc);
	t_men_cpu_umv *men_cpu_umv = crear_men_cpu_umv(CAMBIO_PA, id_proc, 0, 0, NULL);
	socket_send_cpu_umv(socketUmv, men_cpu_umv);
	destruir_men_cpu_umv(men_cpu_umv);
}

void traerIndiceEtiquetas(){
	int32_t base, offset, tam;
	base =pcb->dir_primer_byte_umv_indice_etiquetas;
	offset=0;
	tam=pcb->tam_indice_etiquetas;

	t_men_cpu_umv *sol_etiquetas = crear_men_cpu_umv(SOL_BYTES, base, offset, tam, NULL);
	socket_send_cpu_umv(socketUmv, sol_etiquetas);
	destruir_men_cpu_umv(sol_etiquetas);

	t_men_comun *rec_etiq = socket_recv_comun(socketUmv);
	if(rec_etiq->tipo == R_SOL_BYTES){
		etiquetas = malloc(rec_etiq->tam_dato);
		etiquetas = rec_etiq->dato;
	}
	if(rec_etiq->tipo == SEGMEN_FAULT){
		manejarSegmentationFault();
	}
}

void recibirUnPcb(){
	t_men_quantum_pcb *m = socket_recv_quantum_pcb(socketKernel);
	pcb = malloc(sizeof(t_pcb));
	if((m->tipo==CONEC_CERRADA) || (m->tipo!=PCB_Y_QUANTUM)){
		printf("ERROR se ha perdido la coneccion con el Kernel o el tipo de dato recibido es erroneo\n");
	}else{
		memcpy(pcb, m->pcb, sizeof(t_pcb));
		memcpy(&quantum_max , &m->quantum, sizeof(int32_t));
	}
	destruir_quantum_pcb(m);
}

void crearDiccionario(){
	dic_Variables = dictionary_create();
	int32_t base, offset, tam;
	//si se trata de un programa con un  stack que ya tiene variables
	int32_t tam_contexto = pcb->cant_var_contexto_actual;
	int32_t i;

	if (tam_contexto > 0){//regenerar diccionario
		base = pcb->dir_primer_byte_umv_contexto_actual;
		for(i=0;i<tam_contexto;i++){
			offset= (i)*5;
			tam=1;
			t_men_cpu_umv *sol_var = crear_men_cpu_umv(SOL_BYTES, base, offset, tam, NULL);
			socket_send_cpu_umv(socketUmv, sol_var);
			destruir_men_cpu_umv(sol_var);

			t_men_comun *rec_var = socket_recv_comun(socketUmv);

			if(rec_var->tipo == SEGMEN_FAULT){
				manejarSegmentationFault();
			}
			if(rec_var->tipo== R_SOL_BYTES){
				char *key = malloc(2);
				key[0] = rec_var->dato[0];
				key[1] = '\0';
				int32_t *pos = malloc(sizeof(int32_t));
				*pos = base + offset;
				dictionary_put(dic_Variables, key, pos );
			}
			destruir_men_comun(rec_var);
		}
	}
}

void manejarSegmentationFault(){
	//informar por pantalla el error
	printf("Hubo un error en la solictud de memoria, se finalizará la ejecucion del programa actual %d\n", pcb->id);
	txt_write_in_file(cpu_file_log, "Hubo un error en la solictud de memoria, se finalizará la ejecucion del programa actual\n");
	//y concluir la ejecucion del programa actual

	quantum_actual++;
	fueFinEjecucion =0;//todo puede q sea 1

	t_men_quantum_pcb *error_umv = crear_men_quantum_pcb(SEGMEN_FAULT, 0, pcb);
	socket_send_quantum_pcb(socketKernel, error_umv);
	destruir_quantum_pcb(error_umv);
	//como lo haria cuando el analizador de linea
}

char* solicitarProxSentenciaAUmv(){// revisar si de hecho devuelve la prox instruccion(calculos)
	int32_t base;
	int32_t *offset = malloc(sizeof(int32_t));
	int32_t *tam = malloc(sizeof(int32_t));;
	char* proxInst = NULL;
	t_men_cpu_umv *men_sol_inst;
	t_men_comun *men_base_offset;
	//mandarle a la umv el cambio de proceso activo  pcb->id
	//con el indice de codigo y el pc obtengo la posicion de la proxima instruccion a ejecutar

	men_sol_inst = crear_men_cpu_umv(SOL_BYTES, pcb->dir_primer_byte_umv_indice_codigo, 8*pcb->program_counter, 8, NULL);
	socket_send_cpu_umv(socketUmv, men_sol_inst);
	destruir_men_cpu_umv(men_sol_inst);

	men_base_offset = socket_recv_comun(socketUmv);
	if((men_base_offset->tam_dato != 8) || (men_base_offset->tipo!=R_SOL_BYTES))//por si la UMV me llega a mandar un tamanio distinto al q pedi o si el tipo de dato es diferente
		printf("ERROR el tamanio recibido:%i es distinto a 8, o el tipo de dato:%i es distinto a R_SOL_BYTES\n",men_base_offset->tam_dato,men_base_offset->tipo);

	memcpy(offset, men_base_offset->dato, 4);
	memcpy(tam, men_base_offset->dato+4, 4);
	destruir_men_comun(men_base_offset);

	base = pcb->dir_primer_byte_umv_segmento_codigo;

	//le mando a la umv base, offset y tamanio de la proxima instruccion
	men_sol_inst = crear_men_cpu_umv(SOL_BYTES, base, *offset, *tam, NULL);
	socket_send_cpu_umv(socketUmv, men_sol_inst);
	destruir_men_cpu_umv(men_sol_inst);

	//recibir la instruccion a ejecutar
	t_men_comun *rec_inst = socket_recv_comun(socketUmv);

	if(rec_inst->tipo == R_SOL_BYTES){
		proxInst = malloc(rec_inst->tam_dato);
		memcpy(proxInst, rec_inst->dato,rec_inst->tam_dato);
		txt_write_in_file(cpu_file_log, "Instruccion que voy a ejecutar\n");
		printf("Instruccion que voy a ejecutar:%s\n",proxInst);
		pcb->program_counter ++;
		destruir_men_comun(rec_inst);
		return proxInst;
	}
	if(rec_inst->tipo ==SEGMEN_FAULT){
		manejarSegmentationFault();
		destruir_men_comun(rec_inst);
		return proxInst;
	}
	printf("ERROR tipo de dato:%i, recibido es erroneo\n", rec_inst->tipo);
	return proxInst;
}

void salirPorQuantum(){
	//mando el pcb con un tipo de mensaje que sali por quantum
	t_men_quantum_pcb *p=crear_men_quantum_pcb(FIN_QUANTUM,0, pcb);
	socket_send_quantum_pcb(socketKernel, p);
	destruir_quantum_pcb(p);
}

void signal_handler(int sig){
	got_usr1 =1;

	t_men_quantum_pcb *m = crear_men_quantum_pcb(CPU_DESCONEC, 0, pcb);
	socket_send_quantum_pcb(socketKernel, m);
	destruir_quantum_pcb(m);

	fueFinEjecucion = 1;

	txt_write_in_file(cpu_file_log, "Se recibio la SIGUSR1 \n");
	printf("Se recibio la SIGUSR1");

}

void preservarContexto(){
	int32_t base, offset, tam;
	base = pcb->dir_primer_byte_umv_contexto_actual;
	offset = (pcb->cant_var_contexto_actual)+1;	// la posicion siguiente al ultimo valor de la ultima varaible
	tam = sizeof(int32_t);
	char * buffer = malloc(tam);
	buffer = string_itoa(base);

	t_men_cpu_umv *save_context = crear_men_cpu_umv(ALM_BYTES,base, offset, tam, buffer);
	socket_send_cpu_umv(socketUmv, save_context);
	destruir_men_cpu_umv(save_context);

	t_men_comun *r_con = socket_recv_comun(socketUmv);
	if (r_con->tipo == R_ALM_BYTES){
		offset = offset + 4;
		buffer = string_itoa(pcb->program_counter);
		t_men_cpu_umv *save_proxins = crear_men_cpu_umv(ALM_BYTES, base, offset, tam, buffer);
		socket_send_cpu_umv(socketUmv, save_proxins);
		destruir_men_cpu_umv(save_proxins);

		t_men_comun *r_proxins = socket_recv_comun(socketUmv);
		if(r_proxins->tipo == R_ALM_BYTES){
			txt_write_in_file(cpu_file_log, "Se almaceno correctamente el contexto\n");
			printf("Se almaceno correctamente el contexto\n");
		}
		if(r_proxins->tipo == SEGMEN_FAULT){
			manejarSegmentationFault();
		}
		destruir_men_comun(r_proxins);
		free(buffer);
	}
	if(r_con->tipo == SEGMEN_FAULT){
		manejarSegmentationFault();
	}
	destruir_men_comun(r_con);

}

void finalizarContexto(){//todo si finaliza por un error es distinto???

	//todo creo q tendria q decirle al kernel, q el programa imprima todas las variables

	t_men_comun *men_fin = crear_men_comun(FIN_EJECUCION, string_itoa(pcb->id), sizeof(int32_t));
	socket_send_comun(socketKernel, men_fin);
	destruir_men_comun(men_fin);

	fueFinEjecucion = 1;

	txt_write_in_file(cpu_file_log, "Mandando a Kernel-PCP el pcb de un programa que se termino de ejecutar\n");
}

//Primitivas ANSISOP
t_puntero definirVariable(t_nombre_variable identificador_variable){
	int32_t base, offset, tam;
	base = pcb->dir_primer_byte_umv_contexto_actual;
	offset= (pcb->cant_var_contexto_actual)*5;

	tam = sizeof(int32_t)+1;
	int32_t pos_mem = base + offset;

	printf("Definir la variable %c en la posicion %i, offset:%i\n",identificador_variable, pos_mem,offset);
	char *buffer=malloc(tam);
	buffer[0]=identificador_variable;
	t_men_cpu_umv *alm_bytes = crear_men_cpu_umv(ALM_BYTES, base, offset,tam , buffer);
	socket_send_cpu_umv(socketUmv, alm_bytes);
	destruir_men_cpu_umv(alm_bytes);

	t_men_comun *r_alm = socket_recv_comun(socketUmv);

	if(r_alm->tipo == R_ALM_BYTES){
		char *string_id_var = malloc(2);
		string_id_var[0] =identificador_variable;
		string_id_var[1]='\0';
		int32_t *pos = malloc(sizeof(int32_t));
		*pos = pos_mem;
		dictionary_put(dic_Variables, string_id_var, pos);
		(pcb->cant_var_contexto_actual) ++;
		txt_write_in_file(cpu_file_log, "Definiar la variable \n");
		txt_write_in_file(cpu_file_log, "en la posicion \n");
		destruir_men_comun(r_alm);
		return pos_mem;
	}
	if(r_alm->tipo == SEGMEN_FAULT){
		manejarSegmentationFault();
		base=0;
		offset=0;
		destruir_men_comun(r_alm);
		return 0;
	}
	printf("ERROR tipo de dato:%i, recibido es erroneo\n", r_alm->tipo);
	destruir_men_comun(r_alm);
	return -1;
}

t_puntero obtenerPosicionVariable(t_nombre_variable identificador_variable){
	/*Se fija en el diccionario de variables la posicion la posicion va a ser el data del elemento*/
	char key[2];
	key[0] = identificador_variable;
	key[1] = '\0';

	int32_t *e = dictionary_get(dic_Variables,key);

	if (e == NULL)
		printf("ERROR no se ha podido encontrar la ket:%s\n",key);

	txt_write_in_file(cpu_file_log, "La posicion de la variable\n");
	txt_write_in_file(cpu_file_log, "es\n");
	printf("La posicion de la variable: %s, es %i\n", key, *e);

	return *e;
}

t_valor_variable dereferenciar(t_puntero direccion_variable){
	//obtiene los cuatro bytes siguientes a direccion_variable
	t_valor_variable valor;

	int32_t base = pcb->dir_primer_byte_umv_contexto_actual;
	int32_t offset = direccion_variable-base+1;//todo creo q esto es asi hay q fijarse	// son los cuatro bytes siguientes a la variable
	int32_t tam= sizeof(int32_t);

	t_men_cpu_umv *sol_val = crear_men_cpu_umv(SOL_BYTES,base, offset, tam, NULL);

	socket_send_cpu_umv(socketUmv,sol_val);
	destruir_men_cpu_umv(sol_val);

	//recibir de umv 4 bytes de valor_variable
	t_men_comun *men_comun = socket_recv_comun(socketUmv);

	if (men_comun->tipo == R_SOL_BYTES){
		memcpy(&valor, men_comun->dato,men_comun->tam_dato);
		txt_write_in_file(cpu_file_log, "Dereferenciar \n");
		txt_write_in_file(cpu_file_log, "y su valor es \n");
		printf("Dereferenciar %d, offset:%i y su valor es: %d\n",direccion_variable, offset,valor);
	}
	if(men_comun->tipo ==SEGMEN_FAULT){
		manejarSegmentationFault();
	}
	return valor;
}

void asignar(t_puntero direccion_variable, t_valor_variable valor){
	//envio a la umv para almacenar base= contexto actual offset= direccion_variable tamaño=4bytes buffer= valor
	int32_t base = pcb->dir_primer_byte_umv_contexto_actual;
	int32_t offset = direccion_variable-base+1;

	int32_t tam = sizeof(t_valor_variable);
	char* buffer = malloc(tam);
	memcpy(buffer, &valor,tam);

	t_men_cpu_umv  *men_asig= crear_men_cpu_umv(ALM_BYTES, base, offset, tam, buffer);
	socket_send_cpu_umv(socketUmv, men_asig);
	destruir_men_cpu_umv(men_asig);

	t_men_comun *r_alm = socket_recv_comun(socketUmv);

	if(r_alm->tipo == R_ALM_BYTES){
		txt_write_in_file(cpu_file_log, "Asignando en la direccion \n");
		txt_write_in_file(cpu_file_log, "el valor \n");
		printf("Asignando en la direccion %d, offset:%i, el valor %d \n", direccion_variable, offset, valor);
	}
	if(r_alm->tipo == SEGMEN_FAULT){
		manejarSegmentationFault();
	}
}

t_valor_variable obtenerValorCompartida(t_nombre_compartida variable){
	//le mando al kernel el nombre de la variable compartida
	t_men_comun *m = crear_men_comun(OBTENER_VALOR, variable, sizeof(variable));
	socket_send_comun(socketKernel, m);

	//recibo el valor
	t_men_comun *respuesta = socket_recv_comun(socketKernel);

	t_valor_variable valor;
	memcpy(&valor, respuesta->dato,respuesta->tam_dato);

	txt_write_in_file(cpu_file_log, "Obteniendo el valor de compartida \n");
	txt_write_in_file(cpu_file_log, "el valor \n");
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

	destruir_men_comun(var);
	destruir_men_comun(val_asignado);

	txt_write_in_file(cpu_file_log, "Asignando a variable compartida \n");
	printf("Asignando a variable compartida %s el valor %d \n", variable, valor);
	return valor;
	}

void irAlLabel(t_nombre_etiqueta t_nombre_etiqueta){//revisar
	// primer instruccion ejecutable de etiqueta y -1 en caso de error
	t_puntero_instruccion pos_etiqueta= metadata_buscar_etiqueta(t_nombre_etiqueta, etiquetas, pcb->tam_indice_etiquetas);

	if(pos_etiqueta != -1){
		pcb->program_counter = pos_etiqueta;
	}else{
		txt_write_in_file(cpu_file_log, "Hubo un error en la busqueda de la etiqueta\n");
		printf("Hubo un error en la busqueda de la etiqueta \n");
		finalizarContexto();
	}

	txt_write_in_file(cpu_file_log, "Yendo al label \n");
	printf("Yendo al label %s\n", t_nombre_etiqueta);

}

void llamarSinRetorno(t_nombre_etiqueta etiqueta){
	/*Preserva el contexto de ejecución actual para poder retornar luego al mismo.*/
	preservarContexto();


	pcb->cant_var_contexto_actual = 0;
	pcb->dir_primer_byte_umv_contexto_actual = (pcb->dir_primer_byte_umv_contexto_actual)+5*(pcb->cant_var_contexto_actual)+1;

	irAlLabel(etiqueta);

	txt_write_in_file(cpu_file_log, "Llamando sin retorno a etiqueta\n");
	printf("Llamando sin retorno a etiqueta %s\n", etiqueta);

	}

void llamarConRetorno(t_nombre_etiqueta etiqueta, t_puntero donde_retornar){
	int32_t base, offset, tam;
	//preservar el contexto
	preservarContexto();
	base = pcb->dir_primer_byte_umv_contexto_actual;
	offset = base + (pcb->cant_var_contexto_actual)+1;
	tam = sizeof(int32_t);

	offset = offset + 4;
	char* buffer = malloc(sizeof(int32_t));
	buffer = string_itoa(donde_retornar);
	t_men_cpu_umv *save_retorno = crear_men_cpu_umv(ALM_BYTES, base, offset, tam, buffer);
	socket_send_cpu_umv(socketUmv, save_retorno);
	destruir_men_cpu_umv(save_retorno);

	free(buffer);

	t_men_comun *r_alm = socket_recv_comun(socketUmv);
	if(r_alm->tipo == R_ALM_BYTES){
		txt_write_in_file(cpu_file_log, "Se almaceno la direccion de retorno \n");
		printf("Se almaceno la direccion de retorno\n");

		pcb->cant_var_contexto_actual = 0;
		pcb->dir_primer_byte_umv_contexto_actual = (pcb->dir_primer_byte_umv_contexto_actual)+5*(pcb->cant_var_contexto_actual)+1;
		irAlLabel(etiqueta);

		txt_write_in_file(cpu_file_log, "Llamando con retorno a la etiqueta\n");
		printf("Llamando con retorno a etiqueta %s y retorna en %d", etiqueta, donde_retornar);

	}
	if(r_alm->tipo == SEGMEN_FAULT){
		manejarSegmentationFault();
	}



}

void finalizar(void){
	int32_t base, offset, tam;
	base=pcb->dir_primer_byte_umv_contexto_actual;

	//si es el fin de programa, le digo al kernel q finalizo y salgo de la funcion
	if(base == pcb->dir_primer_byte_umv_segmento_stack){
		finalizarContexto();
		txt_write_in_file(cpu_file_log, "Finalizando el programa actual\n");
		printf("Finalizando el programa actual\n");
		return;
	}
	printf("Finalizando sssssssssSSel programa actual\n");
	offset= base -4;//todo aca el offset es MUY grande
	tam=sizeof(int32_t);

	t_men_cpu_umv *sol_progcount = crear_men_cpu_umv(SOL_BYTES, base, offset, tam, NULL);
	socket_send_cpu_umv(socketUmv, sol_progcount);
	destruir_men_cpu_umv(sol_progcount);

	t_men_comun *rec_progcount= socket_recv_comun(socketUmv);
	if(rec_progcount->tipo == R_SOL_BYTES){
		int32_t prog_counter;
		memcpy(&prog_counter, rec_progcount->dato,rec_progcount->tam_dato);
		pcb->program_counter= prog_counter;
	}
	if(rec_progcount->tipo == SEGMEN_FAULT){
		manejarSegmentationFault();
	}

	offset= offset-4;

	t_men_cpu_umv *sol_context = crear_men_cpu_umv(SOL_BYTES, base, offset, tam, NULL);
	socket_send_cpu_umv(socketUmv, sol_context);
	destruir_men_cpu_umv(sol_context);

	t_men_comun *rec_context= socket_recv_comun(socketUmv);
	if(rec_context->tipo == R_SOL_BYTES){

		int32_t context;
		memcpy(&context, rec_context->dato,rec_context->tam_dato);
		pcb->dir_primer_byte_umv_contexto_actual = context;
	}

	if(rec_context->tipo == SEGMEN_FAULT){
		manejarSegmentationFault();
	}
}

void retornar(t_valor_variable retorno){
	int32_t base, offset, tam;
	int32_t dir_retorno = 0;
	int32_t context = 0;

	base=pcb->dir_primer_byte_umv_contexto_actual;
	offset= base -4;
	tam=sizeof(int32_t);


	t_men_cpu_umv *sol_ret = crear_men_cpu_umv(SOL_BYTES,base, offset, tam, NULL);
	socket_send_cpu_umv(socketUmv, sol_ret);
	destruir_men_cpu_umv(sol_ret);

	t_men_comun *rec_ret= socket_recv_comun(socketUmv);
	if(rec_ret->tipo == R_SOL_BYTES){
		memcpy(&dir_retorno, rec_ret->dato,rec_ret->tam_dato);
	}

	if (rec_ret->tipo == SEGMEN_FAULT){
		manejarSegmentationFault();
	}


	offset= offset-4;

	t_men_cpu_umv *sol_progcount = crear_men_cpu_umv(SOL_BYTES, base, offset, tam, NULL);
	socket_send_cpu_umv(socketUmv, sol_progcount);
	destruir_men_cpu_umv(sol_progcount);

	t_men_comun *rec_progcount= socket_recv_comun(socketUmv);
	if(rec_progcount->tipo == R_SOL_BYTES){
		int32_t prog_counter;
		memcpy(&prog_counter, rec_progcount->dato,rec_progcount->tam_dato);
		pcb->program_counter= prog_counter;
	}

	if(rec_progcount->tipo == SEGMEN_FAULT){
		manejarSegmentationFault();
	}

	offset= offset-4;

	t_men_cpu_umv *sol_context = crear_men_cpu_umv(SOL_BYTES, base, offset, tam, NULL);
	socket_send_cpu_umv(socketUmv, sol_context);
	destruir_men_cpu_umv(sol_context);

	t_men_comun *rec_context= socket_recv_comun(socketUmv);
	if(rec_context->tipo == R_SOL_BYTES){
		int32_t context;
		memcpy(&context, rec_context->dato,rec_context->tam_dato);
		pcb->dir_primer_byte_umv_contexto_actual = context;

	}
	if (rec_context->tipo == SEGMEN_FAULT){
		manejarSegmentationFault();
	}


	base = context;
	offset= dir_retorno +1;
	char* buffer = string_itoa(retorno);
	t_men_cpu_umv *alm_val = crear_men_cpu_umv(ALM_BYTES, base, offset, tam, buffer);
	socket_send_cpu_umv(socketUmv, alm_val);
	destruir_men_cpu_umv(alm_val);

	t_men_comun *rec_alm = socket_recv_comun(socketUmv);
	if(rec_alm->tipo == SEGMEN_FAULT){
		manejarSegmentationFault();
	}
	if(rec_alm->tipo == R_ALM_BYTES){
		txt_write_in_file(cpu_file_log, "Retornando valor de variable\n");
		printf("Retornando valor de variable%d en la direccion %d\n", retorno, dir_retorno);
	}

}

void imprimir(t_valor_variable valor_mostrar){
	char *aux = string_itoa(valor_mostrar);
	t_men_comun *men = crear_men_comun(IMPRIMIR_VALOR,aux ,string_length(aux));
	socket_send_comun(socketKernel,men);

	men = crear_men_comun(ID_PROG,string_itoa(pcb->id) ,string_length(string_itoa(pcb->id)));
	socket_send_comun(socketKernel,men);
	destruir_men_comun(men);

	txt_write_in_file(cpu_file_log, "Imprimiendo valor de variable\n");
	printf("Imprimiendo valor de variable %d\n", valor_mostrar);
}

void imprimirTexto(char* texto){
	t_men_comun *men = crear_men_comun(IMPRIMIR_TEXTO, texto,string_length(texto));
	socket_send_comun(socketKernel,men);

	men = crear_men_comun(ID_PROG,string_itoa(pcb->id) ,string_length(string_itoa(pcb->id)));
	socket_send_comun(socketKernel,men);
	destruir_men_comun(men);

	txt_write_in_file(cpu_file_log, "Imprimiendo texto\n");
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
	destruir_quantum_pcb(mpcb);
	//romper no violentamente con ### inexistente
	txt_write_in_file(cpu_file_log, "Saliendo a i/o en dispositivo\n");
	printf("Saliendo a entrada/salida en dispositivo %s por esta cantidad de tiempo%d\n", dispositivo, tiempo);
}

void wait(t_nombre_semaforo identificador_semaforo){
	t_men_comun *men = crear_men_comun(WAIT, identificador_semaforo,string_length(identificador_semaforo));
	socket_send_comun(socketKernel,men);
	destruir_men_comun(men);
	//romper no violentamente con ### inexistente
	txt_write_in_file(cpu_file_log, "Haciendo wait a semaforo\n");

	printf("Haciendo wait a semaforo%s\n", identificador_semaforo);
}

void mi_signal(t_nombre_semaforo identificador_semaforo){
	t_men_comun *men = crear_men_comun(SIGNAL, identificador_semaforo,string_length(identificador_semaforo));
	socket_send_comun(socketKernel,men);
	destruir_men_comun(men);
	//romper no violentamente con ### inexistente
	txt_write_in_file(cpu_file_log,"Haciendo signal a semaforo\n");

	printf("Haciendo signal a semaforo %s\n", identificador_semaforo);

}

void logear_int(FILE* destino,int32_t un_int){
	char *aux_string = string_itoa(un_int);
	txt_write_in_file(destino,aux_string);
	free(aux_string);
}

void logear_char(FILE* destino,char un_char){
	if (un_char == '\0'){
		char *aux_string = "\\0";
		txt_write_in_file(destino, aux_string);
	}else{
		char *aux_string = string_itoa((int)un_char);
		txt_write_in_file(destino, aux_string);
		free(aux_string);
	}
	txt_write_in_file(destino,"-");
}

void handshake_umv(char *puerto, char *ip){
	//envio a la UMV
	socketUmv = socket_crear_client(puerto,ip);
	t_men_comun *mensaje_inicial = crear_men_comun(HS_CPU,"",string_length(""));
	socket_send_comun(socketUmv,mensaje_inicial);
	//espero coneccion de la UMV
	mensaje_inicial = socket_recv_comun(socketUmv);
	if(mensaje_inicial->tipo == HS_UMV){
		txt_write_in_file(cpu_file_log, "UMV conectada \n");
		printf("UMV conectada\n");
	}else{
		txt_write_in_file(cpu_file_log, "ERROR HANDSHAKE UMV\n");
		printf("ERROR HANDSHAKE\n");
	}
}

void handshake_kernel(char *puerto, char *ip){
	//envio al kernel
	socketKernel = socket_crear_client(puerto,ip);
	t_men_comun *mensaje_inicial = crear_men_comun(HS_CPU,"",string_length(""));
	socket_send_comun(socketKernel,mensaje_inicial);
	//espero conexion de kernel
	mensaje_inicial = socket_recv_comun(socketKernel);
	if(mensaje_inicial->tipo == HS_KERNEL){
		txt_write_in_file(cpu_file_log, "Kernel conectado\n");
		printf("Kernel conectado\n");
	}else{
		txt_write_in_file(cpu_file_log, "ERROR HANDSHAKE KERNEL \n");
		printf("ERROR HANDSHAKE\n");
	}
}

void imprimo_config(char *puertoKernel, char *ipKernel, char *puertoUmv, char *ipUmv){
	printf("\n\n------------------------------Archivo Config----------------------------------------\n");
	printf("	Puerto Kernel	= %s\n", puertoKernel);
	printf("	IP Kernel	= %s\n", ipKernel);
	printf("	Puerto UMV	= %s\n", puertoUmv);
	printf("	IP UMV		= %s\n", ipUmv);
	printf("------------------------------------------------------------------------------------\n\n");
}
