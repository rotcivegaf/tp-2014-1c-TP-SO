#include "kernel.h"

int soc_umv;
t_colas *colas;
t_queue *cola_cpu;
pthread_mutex_t mutex_new = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_ready = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_block = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_exec = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_exit = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_miltiprog = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_uso_cola_cpu= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_ready_vacia= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cola_cpu_vacia= PTHREAD_MUTEX_INITIALIZER;
sem_t buff_multiprog, libre_multiprog, cont_exit;
int quit_sistema = 1;

int main(void){
	t_datos_config *diccionario_config = levantar_config();
	//semaforos
	crear_cont(&cont_exit , 0);
	crear_cont(&buff_multiprog , 0);
	crear_cont(&libre_multiprog , diccionario_config->multiprogramacion);
	//colas
	cola_cpu = queue_create();
	colas = malloc(sizeof(t_colas));
	colas->cola_new = queue_create();
	colas->cola_ready = queue_create();
	colas->cola_block = queue_create();
	colas->cola_exec = queue_create();
	colas->cola_exit = queue_create();
	//parametros plp pcp
	t_param_plp *param_plp = ini_pram_plp(diccionario_config);
	t_param_pcp *param_pcp = ini_pram_pcp(diccionario_config);

	//hilos
	pthread_t hilo_imp_colas = NULL;
	pthread_t hilo_plp = NULL;
	pthread_t hilo_pcp = NULL;
	pthread_create(&hilo_imp_colas, NULL, imp_colas , NULL);
	pthread_create(&hilo_plp, NULL, plp, (void *)param_plp);
	pthread_create(&hilo_pcp, NULL, pcp, (void *)param_pcp);
	pthread_join(hilo_plp, NULL);
	pthread_join(hilo_pcp, NULL);

	
	free(param_plp);
	return EXIT_SUCCESS;
}

void *plp(t_param_plp *param_plp){
	pthread_t hilo_new_ready = NULL;
	t_param_new_ready *param_new_ready = malloc(sizeof(t_param_new_ready));
	param_new_ready->multiprogramacion = param_plp->max_multiprogramacion;
	pthread_create(&hilo_new_ready, NULL, manejador_new_ready, (void *)param_new_ready);

	int contador_prog = ID_PROG_INI;
	fd_set master;   // conjunto maestro de descriptores de fichero
	fd_set read_fds; // conjunto temporal de descriptores de fichero para select()
	int fdmax;        // número máximo de descriptores de fichero

	handshake_umv(param_plp->ip_umv, param_plp->puerto_umv);

	int listener_prog = socket_crear_server(param_plp->puerto_prog);
	socket_listen(listener_prog);

    int prog_new_fd;

	FD_ZERO(&master);    // borra los conjuntos maestro y temporal
	FD_ZERO(&read_fds);
	FD_SET(listener_prog, &master);

	fdmax = listener_prog; // por ahora es éste

	int i = 0;
	while(quit_sistema){
		read_fds = master; // cópialo
		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
			perror("PLP-select");
			exit(1);
		}
		for(i = 3; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) { // ¡¡tenemos datos!!
				if (i == listener_prog) {
					// gestionar nuevas conexiones
					prog_new_fd = socket_accept(listener_prog);
					handshake_prog(prog_new_fd);
					FD_SET(prog_new_fd, &master); // añadir al conjunto maestro
					if (prog_new_fd > fdmax)// actualizar el máximo
						fdmax = prog_new_fd;
				}else{
					// gestionar datos de un cliente
					t_mensaje *men_cod_prog = socket_recv_serealizado(i);
					if (men_cod_prog->tipo == CONEC_CERRADA) {
						printf("PLP-select: Prog desconectado n°socket %d\n", i);
						mover_pcb_exit(i);
						socket_cerrar(i);
						FD_CLR(i, &master); // eliminar del conjunto maestro
						break;
					}
					printf("PLP-select: nuevo prog con socket n°%i\n", i);
					contador_prog++;
					t_resp_sol_mem *resp_sol = solicitar_mem(men_cod_prog, param_plp->tam_stack,contador_prog);
					if (resp_sol->memoria_insuficiente == MEM_INSUFICIENTE){
						t_mensaje *men_no_hay_mem = malloc(sizeof(t_mensaje));
						men_no_hay_mem->tipo = MEM_INSUFICIENTE;
						men_no_hay_mem->dato = string_itoa(contador_prog);
						socket_send_serealizado(soc_umv, men_no_hay_mem);
						socket_send_serealizado(i, men_no_hay_mem);
						free(men_no_hay_mem);
					}else{
						t_pcb_otros *pcb_otros = malloc(sizeof(t_pcb_otros));
						pcb_otros->n_socket = i;
						pcb_otros->pcb = crear_pcb_escribir_seg_UMV(men_cod_prog,resp_sol, &contador_prog);
						pcb_otros->peso = calcular_peso(men_cod_prog);
						pcb_otros->tipo_fin_ejecucion = -1;
						pthread_mutex_lock(&mutex_new);
						queue_push(colas->cola_new,pcb_otros);
						pthread_mutex_unlock(&mutex_new);
						pthread_mutex_unlock(&mutex_miltiprog);
					}
				}
			}
		}
	sleep(param_plp->retardo);
	}
	return NULL;
}

void *pcp(t_param_pcp *param_pcp){
	int i, fdmax, cpu_new_fd;
	fd_set master, read_fds;
	pthread_t hilo_ready_exec = NULL;
	t_param_ready_exec *param_ready_exec = malloc(sizeof(t_param_ready_exec));
	param_ready_exec->quantum = param_pcp->quantum;
	pthread_create(&hilo_ready_exec, NULL, manejador_ready_exec, (void *)param_ready_exec);

	pthread_t hilo_exit = NULL;
	pthread_create(&hilo_exit, NULL, manejador_exit, NULL);

	int listener_cpu = socket_crear_server(param_pcp->puerto_cpu);
	socket_listen(listener_cpu);
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	FD_SET(listener_cpu, &master);
	fdmax = listener_cpu;
	while(quit_sistema){
		read_fds = master; // cópialo
		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
			perror("PCP-select");
			exit(1);
		}
		for(i = 3; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) {
				if (i == listener_cpu) {
					// gestionar nuevas conexiones
					cpu_new_fd = socket_accept(listener_cpu);
					handshake_cpu(cpu_new_fd);
					t_cpu *cpu = malloc(sizeof(t_cpu));
					cpu->soc_cpu = cpu_new_fd;
					cpu->id_prog_exec = 0;
					pthread_mutex_lock(&mutex_uso_cola_cpu);
					queue_push(cola_cpu, cpu);
					pthread_mutex_unlock(&mutex_uso_cola_cpu);
					pthread_mutex_unlock(&mutex_cola_cpu_vacia);
					printf("PCP-select: CPU conectada n°socket %i\n", cpu_new_fd);
					FD_SET(cpu_new_fd, &master); // añadir al conjunto maestro
					if (cpu_new_fd > fdmax)// actualizar el máximo
						fdmax = cpu_new_fd;
				}else{
					// gestionar datos de un cliente
					t_mensaje *men_cpu = socket_recv_serealizado(i);
					if (men_cpu->tipo == CONEC_CERRADA) {
						printf("PCP-select: CPU desconectada n°socket %i\n", i);
						t_cpu *aux_cpu = get_cpu(i);
						if (aux_cpu->id_prog_exec != 0){
							t_pcb_otros *aux_pcb_otros = get_pcb_otros_exec(aux_cpu->id_prog_exec);
							t_mensaje *men = crear_t_mensaje(CPU_DESCONEC, "", 1);
							socket_send_serealizado(aux_pcb_otros->n_socket, men);
							pthread_mutex_lock(&mutex_exit);
							queue_push(colas->cola_exit, aux_pcb_otros);
							sem_incre(&cont_exit);
							pthread_mutex_unlock(&mutex_exit);
						}
						socket_cerrar(i);
						FD_CLR(i, &master);
						continue;
					}
					if (men_cpu->tipo == FIN_QUANTUM){
						t_pcb_otros *aux_pcb_otros = malloc(sizeof(t_pcb_otros));
						t_mensaje *aux_men_cpu = socket_recv_serealizado(i);
						aux_pcb_otros = deserializar(men_cpu);
						aux_pcb_otros = get_pcb_otros_exec(aux_pcb_otros->pcb->id);
						pthread_mutex_lock(&mutex_ready);
						queue_push(colas->cola_ready,aux_pcb_otros);
						pthread_mutex_unlock(&mutex_ready);
						pthread_mutex_unlock(&mutex_ready_vacia);
					}
					if ((men_cpu->tipo == IMPRIMIR_TEXTO) || (men_cpu->tipo == IMPRIMIR_VALOR)) {
						t_mensaje *aux_men_cpu = socket_recv_serealizado(i);
						if (men_cpu->tipo != ID_PROG)
							printf("Se esperaba el tipo de dato %i y se obtuvo %i\n",ID_PROG,men_cpu->tipo);
						t_pcb_otros *aux_pcb_otros = malloc(sizeof(t_pcb_otros));
						aux_pcb_otros = get_pcb_otros_exec_sin_quitarlo(atoi(aux_men_cpu->dato));
						socket_send_serealizado(aux_pcb_otros->n_socket, men_cpu);
						continue;
					}
					if (men_cpu->tipo == IO_ID){
						enviar_IO(i, param_pcp->cola_IO);
						continue;
					}

				}
			}
		}
	sleep(param_pcp->retardo);
	}

	return NULL;
}

t_pcb_otros *get_pcb_otros_exec_sin_quitarlo(int id_proc){
	t_pcb_otros *aux_pcb_otros = malloc(sizeof(t_pcb_otros));
	int i;

	pthread_mutex_lock(&mutex_exec);
	int tam = queue_size(colas->cola_exec);
	for(i=0 ;i < tam ;i++){
		aux_pcb_otros = queue_pop(colas->cola_exec);
		if (id_proc == aux_pcb_otros->pcb->id){
			queue_push(colas->cola_exec, aux_pcb_otros);
			aux_pcb_otros = queue_peek(colas->cola_exec);
			pthread_mutex_unlock(&mutex_exec);
			return aux_pcb_otros;
		}
		queue_push(colas->cola_exec, aux_pcb_otros);
	}
	printf("ERROR EN get_pcb_otros_exec\n");
	return NULL;
}

int mover_pcb_exit(int soc_prog){
	int i;
	t_pcb_otros *aux = malloc(sizeof(t_pcb_otros));
	pthread_mutex_lock(&mutex_new);
	for(i=0; i < queue_size(colas->cola_new) ;i++){
		aux = queue_pop(colas->cola_new);
		if (aux->n_socket == soc_prog){
			pthread_mutex_lock(&mutex_exit);
			queue_push(colas->cola_exit, aux);
			pthread_mutex_unlock(&mutex_exit);
			pthread_mutex_unlock(&mutex_new);
			sem_incre(&cont_exit);
			return 0;
		}
		queue_push(colas->cola_new, aux);
	}
	pthread_mutex_unlock(&mutex_new);
	pthread_mutex_lock(&mutex_ready);
	for(i=0; i < queue_size(colas->cola_ready) ;i++){
		aux = queue_pop(colas->cola_ready);
		if (aux->n_socket == soc_prog){
			pthread_mutex_lock(&mutex_exit);
			queue_push(colas->cola_exit, aux);
			pthread_mutex_unlock(&mutex_exit);
			pthread_mutex_unlock(&mutex_ready);
			sem_incre(&cont_exit);
			return 0;
		}
		queue_push(colas->cola_ready, aux);
	}
	pthread_mutex_unlock(&mutex_ready);
	pthread_mutex_lock(&mutex_block);
	for(i=0; i < queue_size(colas->cola_block) ;i++){
		aux = queue_pop(colas->cola_block);
		if (aux->n_socket == soc_prog){
			pthread_mutex_lock(&mutex_exit);
			queue_push(colas->cola_exit, aux);
			pthread_mutex_unlock(&mutex_exit);
			pthread_mutex_unlock(&mutex_block);
			sem_incre(&cont_exit);
			return 0;
		}
		queue_push(colas->cola_block, aux);
	}
	pthread_mutex_unlock(&mutex_block);
	pthread_mutex_lock(&mutex_exec);
	for(i=0; i < queue_size(colas->cola_exec) ;i++){
		aux = queue_pop(colas->cola_exec);
		if (aux->n_socket == soc_prog){
			pthread_mutex_lock(&mutex_exit);
			queue_push(colas->cola_exit, aux);
			pthread_mutex_unlock(&mutex_exit);
			pthread_mutex_unlock(&mutex_exec);
			sem_incre(&cont_exit);
			return 0;
		}
		queue_push(colas->cola_exec, aux);
	}
	pthread_mutex_unlock(&mutex_exec);
	return -1;
}

t_cpu *get_cpu(int soc_cpu){
	int i;
	t_cpu *cpu = malloc(sizeof(t_cpu));
	pthread_mutex_lock(&mutex_uso_cola_cpu);
	int tam = queue_size(cola_cpu);

	for(i=0 ;i < tam ;i++){
		cpu = queue_pop(cola_cpu);
		if (cpu->soc_cpu == soc_cpu){
			pthread_mutex_unlock(&mutex_uso_cola_cpu);
			return cpu;
		}
		queue_push(cola_cpu, cpu);
	}
	printf("ERROR EN GET_CPU\n");
	return NULL;
}

t_pcb_otros *get_pcb_otros_exec(int id_proc){
	t_pcb_otros *aux_pcb_otros = malloc(sizeof(t_pcb_otros));
	int i;

	pthread_mutex_lock(&mutex_exec);
	int tam = queue_size(colas->cola_exec);
	for(i=0 ;i < tam ;i++){
		aux_pcb_otros = queue_pop(colas->cola_exec);
		if (id_proc == aux_pcb_otros->pcb->id){
			pthread_mutex_unlock(&mutex_exec);
			return aux_pcb_otros;
		}
		queue_push(colas->cola_exec, aux_pcb_otros);
	}
	printf("ERROR EN get_pcb_otros_exec\n");
	return NULL;
}

void enviar_IO(int soc_cpu, t_queue *cola_IO){
	t_mensaje *men;
	t_IO *aux_IO;
	int i;
	t_param_IO *param_IO = malloc(sizeof(t_param_IO));
	pthread_t hilo_IO = NULL;

	men = socket_recv_serealizado(soc_cpu);
	if (men->tipo != IO_ID)
		printf("ERROR: La CPU mando el tipo %i y se esperaba %i\n",men->tipo,IO_ID);
	for (i=0; i < queue_size(cola_IO); i++){
		aux_IO = queue_pop(cola_IO);
		if (strcmp(aux_IO->id_hio, men->dato) == 0){
			param_IO->retardo = aux_IO->hio_sleep;
		}else{
			printf("ERROR: no existe el IO\n");
		}
		queue_push(cola_IO, aux_IO);
	}
	men = socket_recv_serealizado(soc_cpu);
	if (men->tipo != IO_CANT_UNIDADES)
		printf("ERROR: La CPU mando el tipo %i y se esperaba %i\n",men->tipo,IO_CANT_UNIDADES);
	param_IO->cant_unidades = atoi(men->dato);

	t_cpu *aux_cpu = malloc(sizeof(t_cpu));
	int tam = queue_size(cola_cpu);
	int aux_id_procAbloc;
	pthread_mutex_lock(&mutex_uso_cola_cpu);
	for(i=0 ;i < tam ;i++){
		aux_cpu = queue_pop(cola_cpu);
		if (aux_cpu->soc_cpu == soc_cpu){
			aux_id_procAbloc = aux_cpu->id_prog_exec;
			aux_cpu->id_prog_exec = 0;
			i = tam;
		}
		queue_push(cola_cpu, aux_cpu);
	}
	pthread_mutex_unlock(&mutex_uso_cola_cpu);
	pthread_mutex_unlock(&mutex_cola_cpu_vacia);

	t_pcb_otros *aux_pcb_otros = malloc(sizeof(t_pcb_otros));
	pthread_mutex_lock(&mutex_exec);
	for(i=0 ;i < queue_size(colas->cola_exec) ;i++){
		aux_pcb_otros = queue_pop(colas->cola_exec);
		if (aux_pcb_otros->pcb->id == aux_id_procAbloc){
			pthread_mutex_lock(&mutex_block);
			queue_push(colas->cola_block,aux_pcb_otros);
			pthread_mutex_unlock(&mutex_block);
			param_IO->id_proc = aux_pcb_otros->pcb->id;
		}
		queue_push(colas->cola_exec, aux_pcb_otros);
	}
	pthread_mutex_unlock(&mutex_exec);
	pthread_create(&hilo_IO, NULL, entrar_IO, (void *)param_IO);
}

t_pcb_otros *get_peso_min(){
	int i, peso_min = -1;
	t_pcb_otros *aux = malloc(sizeof(t_pcb_otros));
	int tam_cola = queue_size(colas->cola_new);

	for (i=0;i < tam_cola;i++){
		aux = queue_pop(colas->cola_new);
		if(peso_min < aux->peso)
			peso_min = aux->peso;
		queue_push(colas->cola_new,aux);
	}
	for (i=0;i < tam_cola;i++){
		aux = queue_pop(colas->cola_new);
		if (peso_min == aux->peso)
			return aux;
		queue_push(colas->cola_new,aux);
	}
	return NULL;
}

void *entrar_IO(t_param_IO *param){
	int	total = param->retardo* param->cant_unidades;
	sleep(total);
	int i;
	t_pcb_otros *aux_pcb_otros = malloc(sizeof(t_pcb_otros));
	pthread_mutex_unlock(&mutex_block);
	for(i=0 ;i < queue_size(colas->cola_block) ;i++){
		aux_pcb_otros = queue_pop(colas->cola_block);
		if (aux_pcb_otros->pcb->id == param->id_proc){
			pthread_mutex_unlock(&mutex_ready);
			queue_push(colas->cola_ready,aux_pcb_otros);
			pthread_mutex_unlock(&mutex_ready);
		}
		queue_push(colas->cola_block, aux_pcb_otros);
	}
	pthread_mutex_unlock(&mutex_block);

	return NULL;
}

void *manejador_exit(){
	t_pcb_otros *aux_pcb_otros;
	int soc_prog;
	t_mensaje *men;
	while(quit_sistema){
		pthread_mutex_lock(&mutex_exit);
		if (queue_is_empty(colas->cola_exit)){
			pthread_mutex_unlock(&mutex_exit);
			sem_decre(&cont_exit);
		}else{
			aux_pcb_otros = queue_pop(colas->cola_exit);
			pthread_mutex_unlock(&mutex_exit);
			if (aux_pcb_otros->tipo_fin_ejecucion == CPU_DESCONEC){
				men = crear_t_mensaje(aux_pcb_otros->tipo_fin_ejecucion,"",1);
				soc_prog = aux_pcb_otros->n_socket;
				socket_send_serealizado(soc_prog , men);
			}
			umv_destrui_pcb(aux_pcb_otros->pcb);
			free(aux_pcb_otros->pcb);
			free (aux_pcb_otros);
		}
	}
	return NULL;
}

void *manejador_new_ready(t_param_new_ready *param){
	while(quit_sistema){
		pthread_mutex_lock(&mutex_new);
		if (queue_is_empty(colas->cola_new)){
			pthread_mutex_unlock(&mutex_new);
			pthread_mutex_lock(&mutex_miltiprog);
		}else{
			pthread_mutex_unlock(&mutex_new);
			sem_decre(&libre_multiprog);
			pthread_mutex_lock(&mutex_new);
			pthread_mutex_lock(&mutex_ready);
			queue_push(colas->cola_ready, get_peso_min());
			pthread_mutex_unlock(&mutex_new);
			pthread_mutex_unlock(&mutex_ready);
			pthread_mutex_unlock(&mutex_ready_vacia);
			sem_incre(&buff_multiprog);
		}
	}
	return NULL;
}

void *manejador_ready_exec(t_param_ready_exec *param){
	t_pcb_otros *aux_pcb_otros;
	t_cpu *cpu = malloc(sizeof(t_cpu));
	int res;
	while(quit_sistema){
		pthread_mutex_lock(&mutex_ready);
		if (queue_is_empty(colas->cola_ready)){
			pthread_mutex_unlock(&mutex_ready);
			pthread_mutex_lock(&mutex_ready_vacia);
		}else{
			pthread_mutex_lock(&mutex_uso_cola_cpu);
			cpu = get_cpu_libre(&res);
			if(res == 0){
				pthread_mutex_unlock(&mutex_ready);
				pthread_mutex_unlock(&mutex_uso_cola_cpu);
				pthread_mutex_lock(&mutex_cola_cpu_vacia);
			}else{
				aux_pcb_otros = queue_pop(colas->cola_ready);
				pthread_mutex_unlock(&mutex_ready);
				socket_send_pcb(cpu->soc_cpu,aux_pcb_otros->pcb , param->quantum);
				cpu->id_prog_exec = aux_pcb_otros->pcb->id;
				queue_push(cola_cpu, cpu);
				pthread_mutex_unlock(&mutex_uso_cola_cpu);
				pthread_mutex_lock(&mutex_exec);
				queue_push(colas->cola_exec, aux_pcb_otros);
				pthread_mutex_unlock(&mutex_exec);
			}
		}
	}
	return NULL;
}

t_cpu *get_cpu_libre(int *res){
	int i;
	t_cpu *cpu = malloc(sizeof(t_cpu));
	int tam = queue_size(cola_cpu);
	for(i=0 ;i < tam ;i++){
		cpu = queue_pop(cola_cpu);
		if (cpu->id_prog_exec == 0){
			*res = 1;
			return cpu;
		}
		queue_push(cola_cpu, cpu);
	}
	*res = 0;
	return cpu;
}

void umv_destrui_pcb(t_pcb *pcb){
	t_mensaje *men_dest;
	men_dest = crear_t_mensaje(DESTR_SEGS, string_itoa(pcb->id),string_length(string_itoa(pcb->id)));
	socket_send_serealizado(soc_umv, men_dest);
}

t_pcb *crear_pcb_escribir_seg_UMV(t_mensaje *men_cod_prog ,t_resp_sol_mem *resp_sol ,int *contador_id_programa){
	t_metadata_program *metadata_program = metadata_desde_literal(men_cod_prog->dato);
	t_mensaje* men;
	char *etis = metadata_program->etiquetas;
	socket_send_serealizado(soc_umv, men_cod_prog);

	men = crear_t_mensaje(IND_ETI_FUNC,etis,metadata_program->etiquetas_size);

	socket_send_serealizado(soc_umv, men);
	int tam_total = metadata_program->instrucciones_size*8;
	char *dato = malloc(tam_total);
	memcpy(dato,metadata_program->instrucciones_serializado,tam_total);
	men = crear_t_mensaje(IND_COD,dato,tam_total);
	socket_send_serealizado(soc_umv, men);


	t_pcb *pcb = malloc(sizeof(t_pcb));
	pcb->id = *contador_id_programa;
	pcb->dir_primer_byte_umv_segmento_codigo = resp_sol->dir_primer_byte_umv_segmento_codigo;
	pcb->dir_primer_byte_umv_segmento_stack = resp_sol->dir_primer_byte_umv_segmento_stack;
	pcb->dir_primer_byte_umv_contexto_actual = resp_sol->dir_primer_byte_umv_segmento_stack;
	pcb->dir_primer_byte_umv_indice_codigo = resp_sol->dir_primer_byte_umv_indice_codigo;
	pcb->dir_primer_byte_umv_indice_etiquetas = resp_sol->dir_primer_byte_umv_indice_etiquetas;
	pcb->program_counter = metadata_program->instruccion_inicio;
	pcb->cant_var_contexto_actual = 0;
	pcb->tam_indice_etiquetas = metadata_program->etiquetas_size;

	return pcb;
}

t_resp_sol_mem * solicitar_mem(t_mensaje *men_cod_prog, int tam_stack, int id_prog){
	t_resp_sol_mem *resp_sol = malloc(sizeof(t_resp_sol_mem));
	resp_sol->memoria_insuficiente = 0;
	t_mensaje *resp_mem;
	t_mensaje *ped_mem;
	int tam = 0;
	//pido mem para el codigo del script
	int script_length = string_length(men_cod_prog->dato);
	ped_mem = crear_t_mensaje( id_prog , string_itoa(script_length) , string_length(string_itoa(script_length)));
	socket_send_serealizado(soc_umv, ped_mem);
	t_mensaje *resp_men_cod = socket_recv_serealizado(soc_umv);
	if (resp_men_cod->tipo == MEM_INSUFICIENTE){
		resp_sol->memoria_insuficiente = MEM_INSUFICIENTE;
		return resp_sol;
	}
	if( resp_men_cod->tipo != RESP_MEM_SEG_COD)
		printf("ERROR: se esperaba %i y obtube un %i\n",RESP_MEM_SEG_COD,resp_men_cod->tipo);
	t_dir_mem dir_mem_cod = atoi(resp_men_cod->dato);
	resp_sol->dir_primer_byte_umv_segmento_codigo = dir_mem_cod;

	//pido mem para el indice de etiquetas y funciones
	t_metadata_program* metadata_program = metadata_desde_literal(men_cod_prog->dato);
	tam = (metadata_program->etiquetas_size*8);
	ped_mem = crear_t_mensaje( PED_MEM_IND_ETI , string_itoa(tam),string_length(string_itoa(tam)));
	socket_send_serealizado(soc_umv, ped_mem);
	resp_mem = socket_recv_serealizado(soc_umv);
	if (resp_mem->tipo == MEM_INSUFICIENTE){
		resp_sol->memoria_insuficiente = MEM_INSUFICIENTE;
		return resp_sol;
	}
	if( resp_mem->tipo != RESP_MEM_IND_ETI)
		printf("ERROR: se esperaba %i y obtube un %i\n",RESP_MEM_IND_ETI,resp_mem->tipo);
	resp_sol->dir_primer_byte_umv_indice_etiquetas= atoi(resp_mem->dato);

	//pido mem para el indice de codigo
	tam = (metadata_program->instrucciones_size*8);
	ped_mem = crear_t_mensaje( PED_MEM_IND_COD , string_itoa(tam),string_length(string_itoa(tam)));
	socket_send_serealizado(soc_umv, ped_mem);
	resp_mem = socket_recv_serealizado(soc_umv);
	if (resp_mem->tipo == MEM_INSUFICIENTE){
		resp_sol->memoria_insuficiente = MEM_INSUFICIENTE;
		return resp_sol;
	}
	if( resp_mem->tipo != RESP_MEM_IND_COD)
		printf("ERROR: se esperaba %i y obtube un %i\n",RESP_MEM_IND_COD,resp_mem->tipo);
	resp_sol->dir_primer_byte_umv_indice_codigo = atoi(resp_mem->dato);

	//pido mem para el stack
	ped_mem = crear_t_mensaje( PED_MEM_SEG_STACK , string_itoa(tam_stack),string_length(string_itoa(tam_stack)));
	socket_send_serealizado(soc_umv, ped_mem);
	resp_mem = socket_recv_serealizado(soc_umv);
	if (resp_mem->tipo == MEM_INSUFICIENTE){
		resp_sol->memoria_insuficiente = MEM_INSUFICIENTE;
		return resp_sol;
	}
	if( resp_mem->tipo != RESP_MEM_SEG_STACK)
		printf("ERROR: se esperaba %i y obtube un %i\n",RESP_MEM_SEG_STACK,resp_mem->tipo);
	t_dir_mem dir_mem_stack = atoi(resp_mem->dato);
	resp_sol->dir_primer_byte_umv_segmento_stack = dir_mem_stack;
	return resp_sol;
}

int calcular_peso(t_mensaje *men_cod_prog){
	//int i;
	t_metadata_program *metadata_program = metadata_desde_literal(men_cod_prog->dato);
	/*char**cod_separado_por_lineas =  string_split(men_cod_prog->dato, "\n");
	for(i=0;cod_separado_por_lineas[i] != '\0';i++);
	int cant_total_lineas_codigo = i;
	int cant_etiquetas = metadata_program->cantidad_de_etiquetas;
	int cant_funciones = metadata_program->cantidad_de_funciones;
	return (5 * cant_etiquetas + 3 * cant_funciones + cant_total_lineas_codigo);*/
	return (5 * (metadata_program->cantidad_de_etiquetas) + 3 * (metadata_program->cantidad_de_funciones) + (metadata_program->instrucciones_size));
}

void handshake_umv(char *ip_umv, char *puerto_umv){
	//envio a la UMV
	soc_umv = socket_crear_client(puerto_umv,ip_umv);
	t_mensaje *mensaje_inicial = crear_t_mensaje(HS_KERNEL_UMV,"",string_length(""));
	socket_send_serealizado(soc_umv,mensaje_inicial);
	//espero coneccion de la UMV
	mensaje_inicial = socket_recv_serealizado(soc_umv);
	if(mensaje_inicial->tipo == HS_KERNEL_UMV){
		printf("UMV conectada\n");
	}else{
		printf("ERROR HANDSHAKE");
	}
}

void socket_send_pcb(int soc,t_pcb *pcb,int quantum){
	t_mensaje *men;
	men = crear_t_mensaje(QUANTUM_MAX, string_itoa(quantum),string_length(string_itoa(quantum)));
	socket_send_serealizado(soc,men);
	men = crear_t_mensaje(CANT_VAR_CONTEXTO_ACTUAL, string_itoa(pcb->cant_var_contexto_actual), string_length(string_itoa(pcb->cant_var_contexto_actual)));
	socket_send_serealizado(soc,men);
	men = crear_t_mensaje(DPBU_CONTEXTO_ACTUAL, string_itoa(pcb->dir_primer_byte_umv_contexto_actual), string_length(string_itoa(pcb->dir_primer_byte_umv_contexto_actual)));
	socket_send_serealizado(soc,men);
	men = crear_t_mensaje(DPBU_INDICE_CODIGO, string_itoa(pcb->dir_primer_byte_umv_indice_codigo), string_length(string_itoa(pcb->dir_primer_byte_umv_indice_codigo)));
	socket_send_serealizado(soc,men);
	men = crear_t_mensaje(DPBU_INDICE_ETIQUETAS, string_itoa(pcb->dir_primer_byte_umv_indice_etiquetas), string_length(string_itoa(pcb->dir_primer_byte_umv_indice_etiquetas)));
	socket_send_serealizado(soc,men);
	men = crear_t_mensaje(DPBU_SEGMENTO_CODIGO, string_itoa(pcb->dir_primer_byte_umv_segmento_codigo), string_length(string_itoa(pcb->dir_primer_byte_umv_segmento_codigo)));
	socket_send_serealizado(soc,men);
	men = crear_t_mensaje(DPBU_SEGMENTO_STACK, string_itoa(pcb->dir_primer_byte_umv_segmento_stack), string_length(string_itoa(pcb->dir_primer_byte_umv_segmento_stack)));
	socket_send_serealizado(soc,men);
	men = crear_t_mensaje(ID_PROG, string_itoa(pcb->id), string_length(string_itoa(pcb->id)));
	socket_send_serealizado(soc,men);
	men = crear_t_mensaje(PROGRAM_COUNTER , string_itoa(pcb->program_counter), string_length(string_itoa(pcb->program_counter)));
	socket_send_serealizado(soc,men);
	men = crear_t_mensaje(TAM_INDICE_ETIQUETAS, string_itoa(pcb->tam_indice_etiquetas), string_length(string_itoa(pcb->tam_indice_etiquetas)));
	socket_send_serealizado(soc,men);
}

t_datos_config *levantar_config(){
	int i;
	t_IO *aux_IO;
	char **id_hios = malloc(sizeof(char));
	char **hio_sleeps = malloc(sizeof(char));
	t_config *diccionario_config = config_create("./kernel/kernel_config");
	t_datos_config *ret = malloc(sizeof(t_datos_config));
	ret->puerto_prog = config_get_string_value( diccionario_config, "Puerto_prog");
	ret->puerto_cpu = config_get_string_value( diccionario_config, "Puerto_CPU");
	ret->ip_umv = config_get_string_value( diccionario_config, "IP_UMV");
	ret->puerto_umv = config_get_string_value( diccionario_config, "Puerto_umv");
	ret->cola_IO = queue_create();
	id_hios = config_get_array_value( diccionario_config, "ID_HIO");
	hio_sleeps = config_get_array_value( diccionario_config, "HIO");
	for (i=0; id_hios[i] != '\0'; i++){
		t_IO *new_IO = malloc(sizeof(t_IO));
		new_IO->id_hio = id_hios[i];
		new_IO->hio_sleep = atoi(hio_sleeps[i]);
		queue_push(ret->cola_IO, new_IO);
	}
	ret->semaforos = config_get_array_value( diccionario_config, "SEMAFOROS");
	ret->valor_semaforos = config_get_array_value( diccionario_config, "VALOR_SEMAFORO");
	ret->multiprogramacion = config_get_int_value( diccionario_config, "MULTIPROGRAMACION");
	ret->quantum = config_get_int_value( diccionario_config, "QUANTUM");
	ret->retardo = config_get_int_value( diccionario_config, "RETARDO");
	ret->tam_stack = config_get_int_value( diccionario_config, "TAMANIO_STACK");
	ret->variables_globales = config_get_array_value( diccionario_config, "VARIABLES_GLOBALES");
	printf("\n\n------------------------------Archivo Config----------------------------------------\n");
	printf("Puerto Proc-Prog = %s\n", ret->puerto_prog);
	printf("Puerto CPU = %s\n", ret->puerto_cpu);
	printf("UMV\n");
	printf("	IP     = %s\n", ret->ip_umv);
	printf("	Puerto = %s\n", ret->puerto_umv);
	printf("Entrada/Salida\n");

	printf("	ID    = ");
	for (i=0; i < queue_size(ret->cola_IO); i++){
		aux_IO = queue_pop(ret->cola_IO);
		printf("%s ", aux_IO->id_hio);
		queue_push(ret->cola_IO, aux_IO);
	}
	printf("\n	Sleep = ");
	for (i=0; i < queue_size(ret->cola_IO); i++){
		aux_IO = queue_pop(ret->cola_IO);
		printf("%i ", aux_IO->hio_sleep);
		queue_push(ret->cola_IO, aux_IO);
	}
	printf("\nSemaforos\n");
	for (i=0; ret->semaforos[i] != '\0'; i++){
		printf("	ID    = %s\n", ret->semaforos[i]);
		printf("	Valor = %s\n", ret->valor_semaforos[i]);
	}
	printf("Varias\n");
	printf("	Multiprogramacion = %i\n", ret->multiprogramacion);
	printf("	Quantum = %i\n", ret->quantum);
	printf("	Retardo =  %i\n", ret->retardo);
	printf("	Tamanio Stack = %i\n", ret->tam_stack);
	printf("Variables Globales\n");
	for (i=0; ret->variables_globales[i] != '\0'; i++){
		printf("	ID    = %s\n", ret->variables_globales[i]);
	}
	printf("------------------------------------------------------------------------------------\n\n");
	return ret;
}

void handshake_cpu(int soc){
	t_mensaje *handshake;
	handshake = socket_recv_serealizado(soc);
	if(handshake->tipo != HS_KERNEL_CPU)
		printf("ERROR HANDSHAKE CPU = %i\n",handshake->tipo);

	socket_send_serealizado(soc,handshake);
}

void handshake_prog(int soc){
	t_mensaje *handshake;
	handshake = socket_recv_serealizado(soc);
	if(handshake->tipo != HS_KERNEL_PROG)
		printf("ERROR HANDSHAKE PROG = %i\n",handshake->tipo);

	socket_send_serealizado(soc,handshake);
}

void *imp_colas (){
	int i;
	t_pcb_otros *aux = malloc(sizeof(t_pcb_otros));
	while(quit_sistema){
		sleep(5);
		printf("--COLA-NEW---------------------------------------------------------\n");
		printf("|PID/PESO= ");
		pthread_mutex_lock(&mutex_new);
		if (queue_is_empty(colas->cola_new)){
			printf("*************COLA VACIA********************");
		}else {
			for(i=0; i < queue_size(colas->cola_new) ;i++){
				aux = queue_pop(colas->cola_new);
				printf("%i/%i - ",aux->pcb->id,aux->peso);
				queue_push(colas->cola_new, aux);
			}
		}
		pthread_mutex_unlock(&mutex_new);
		printf("\n");
		printf("--COLA-READY-------------------------------------------------------\n");
		printf("|  PID   = ");
		pthread_mutex_lock(&mutex_ready);
		if (queue_is_empty(colas->cola_ready)){
			printf("*************COLA VACIA********************");
		}else {
			for(i=0; i < queue_size(colas->cola_ready) ;i++){
				aux = queue_pop(colas->cola_ready);
				printf(" %i  - ",aux->pcb->id);
				queue_push(colas->cola_ready, aux);
			}
		}
		pthread_mutex_unlock(&mutex_ready);
		printf("\n");
		printf("--COLA-BLOCK-------------------------------------------------------\n");
		printf("|  PID   = ");
		pthread_mutex_lock(&mutex_block);
		if (queue_is_empty(colas->cola_block)){
			printf("*************COLA VACIA********************");
		}else {
			for(i=0; i < queue_size(colas->cola_block) ;i++){
				aux = queue_pop(colas->cola_block);
				printf(" %i  - ",aux->pcb->id);
				queue_push(colas->cola_block, aux);
			}
		}
		pthread_mutex_unlock(&mutex_block);
		printf("\n");
		printf("--COLA-EXEC--------------------------------------------------------\n");
		printf("|  PID   = ");
		pthread_mutex_lock(&mutex_exec);
		if (queue_is_empty(colas->cola_exec)){
			printf("*************COLA VACIA********************");
		}else {
			for(i=0; i < queue_size(colas->cola_exec) ;i++){
				aux = queue_pop(colas->cola_exec);
				printf(" %i  - ",aux->pcb->id);
				queue_push(colas->cola_exec, aux);
			}
		}
		pthread_mutex_unlock(&mutex_exec);
		printf("\n");
		printf("--COLA-EXIT--------------------------------------------------------\n");
		printf("|  PID   = ");
		pthread_mutex_lock(&mutex_exit);
		if (queue_is_empty(colas->cola_exit)){
			printf("*************COLA VACIA********************");
		}else {
			for(i=0; i < queue_size(colas->cola_exit) ;i++){
				aux = queue_pop(colas->cola_exit);
				printf(" %i  - ",aux->pcb->id);
				queue_push(colas->cola_exit, aux);
			}
		}
		pthread_mutex_unlock(&mutex_exit);
		printf("\n");
		printf("-------------------------------------------------------------------\n");
	}
	return NULL;
}

void crear_cont(sem_t *sem ,int val_ini){
	if(sem_init(sem, 0, val_ini)== -1){
		perror("No se puede crear el semáforo");
		exit(1);
	}
}

void sem_decre(sem_t *sem){
	if (sem_wait(sem) == -1){
		perror("ERROR sem decrementar");
		exit(1);
	}
}

void sem_incre(sem_t *sem){
	if (sem_post(sem) == -1){
		perror("ERROR sem decrementar");
		exit(1);
	}
}

t_param_plp *ini_pram_plp(t_datos_config *diccionario_config){
	t_param_plp *aux = malloc(sizeof(t_param_plp));
	aux->puerto_prog = diccionario_config->puerto_prog;
	aux->ip_umv = diccionario_config->ip_umv;
	aux->puerto_umv = diccionario_config->puerto_umv;
	aux->retardo = diccionario_config->retardo;
	aux->max_multiprogramacion = diccionario_config->multiprogramacion;
	aux->tam_stack = diccionario_config->tam_stack;
	return aux;
}

t_param_pcp *ini_pram_pcp(t_datos_config *diccionario_config){
	t_param_pcp *aux = malloc(sizeof(t_param_pcp));
	aux->cola_IO = diccionario_config->cola_IO;
	aux->max_multiprogramacion = diccionario_config->multiprogramacion;
	aux->puerto_cpu = diccionario_config->puerto_cpu;
	aux->quantum = diccionario_config->quantum;
	aux->retardo = diccionario_config->retardo;
	aux->semaforos = diccionario_config->semaforos;
	aux->valor_semaforos = diccionario_config->valor_semaforos;
	aux->variables_globales = diccionario_config->variables_globales;
	return aux;
}
