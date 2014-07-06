#include "kernel.h"

int32_t soc_umv;
t_colas *colas;  //colas de los cinco estados de los procesos
t_queue *cola_cpu; //cola donde tengo los cpu conectados
pthread_mutex_t mutex_new = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_ready = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_block = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_exec = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_exit = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_miltiprog = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_uso_cola_cpu= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_ready_vacia= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cola_cpu_vacia= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_dispositivos_io= PTHREAD_MUTEX_INITIALIZER;
sem_t buff_multiprog, libre_multiprog, cont_exit;
int32_t quit_sistema = 1; //no seria int?
t_list *dispositivos_IO;
t_dictionary *diccionario_variables;

int main(void){
	diccionario_variables = dictionary_create();
	t_datos_config *diccionario_config = levantar_config();

	//Inicializacion semaforos
	crear_cont(&cont_exit , 0);
	crear_cont(&buff_multiprog , 0);
	crear_cont(&libre_multiprog , diccionario_config->multiprogramacion);

	//Creacion de colas
	cola_cpu = queue_create();
	colas = malloc(sizeof(t_colas));
	colas->cola_new = queue_create();
	colas->cola_ready = queue_create();
	colas->cola_block = queue_create();
	colas->cola_exec = queue_create();
	colas->cola_exit = queue_create();

	//Lista dispositivos
	dispositivos_IO = list_create();

	//parametros plp pcp
	t_param_plp *param_plp = ini_pram_plp(diccionario_config);
	t_param_pcp *param_pcp = ini_pram_pcp(diccionario_config);

	//Creacion de hilos
	pthread_t hilo_imp_colas, hilo_plp, hilo_pcp;
	pthread_create(&hilo_imp_colas, NULL, imp_colas , NULL);
	pthread_create(&hilo_plp, NULL, plp, (void *)param_plp);
	pthread_create(&hilo_pcp, NULL, pcp, (void *)param_pcp);
	pthread_join(hilo_plp, NULL);
	pthread_join(hilo_pcp, NULL); //Por qué no hay un pthread_join(hilo_imp_colas, NULL)?
	
	free(param_plp->puerto_prog);
	free(param_plp->puerto_umv);
	free(param_plp->ip_umv);
	free(param_plp);
	free(param_pcp->puerto_cpu);
	free(param_pcp->semaforos);
	free(param_pcp->valor_semaforos);
	free(param_pcp->variables_globales);
	queue_destroy(param_pcp->cola_IO);
	queue_destroy(colas->cola_block);
	queue_destroy(colas->cola_exec);
	queue_destroy(colas->cola_exit);
	queue_destroy(colas->cola_new);
	queue_destroy(colas->cola_ready);
	queue_destroy(cola_cpu);
	list_destroy(dispositivos_IO);
	return EXIT_SUCCESS;
}

void *plp(t_param_plp *param_plp){

	//Creacion del hilo que maneja el pasaje de pcbs de new a ready
	pthread_t hilo_new_ready;
	t_param_new_ready *param_new_ready = malloc(sizeof(t_param_new_ready));
	param_new_ready->multiprogramacion = param_plp->max_multiprogramacion;
	pthread_create(&hilo_new_ready, NULL, manejador_new_ready, (void *)param_new_ready);

	int32_t contador_prog = 1; //aca no saría int?
	fd_set master;   // conjunto maestro de sockets
	fd_set read_fds; // conjunto temporal de sockets para select()
	int32_t fdmax;        // socket con el mayor valor ACA NO SERIA INT?

	//Conecta con la umv
	handshake_umv(param_plp->ip_umv, param_plp->puerto_umv);

	//Abre un server para escuchar las conexiones de los programas
	int32_t listener_prog = socket_crear_server(param_plp->puerto_prog);

    int32_t prog_new_fd; //aca no es int tmb?

	FD_ZERO(&master);    // borra los conjuntos maestro y temporal de sockets
	FD_ZERO(&read_fds);
	FD_SET(listener_prog, &master);

	fdmax = listener_prog; // por ahora es éste //SI FDMAX ES INT, LISTENER_PROG TMB

	int32_t i; //aca no es int tmb?
	while(quit_sistema){
		read_fds = master; // Copia el conjunto maestro al temporal
		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
			perror("PLP-select");
			exit(1);
		}
		for(i = 3; i <= fdmax; i++) {

			if (FD_ISSET(i, &read_fds)) { // Si hay datos entrantes en el socket i

				if (i == listener_prog) { //Si i es el socket que espera conexiones, es porque hay un nuevo prog

					prog_new_fd = socket_accept(listener_prog);
					handshake_prog(prog_new_fd);
					FD_SET(prog_new_fd, &master);

					if (prog_new_fd > fdmax) // Actualiza el socket maximo
						fdmax = prog_new_fd;

				}else{

					// Sino, es el socket de un programa ya conectado que manda datos
					t_men_comun *men_cod_prog = socket_recv_comun(i);

					if (men_cod_prog->tipo == CONEC_CERRADA) {
						printf("PLP-select: Prog desconectado n°socket %d\n", i);
						mover_pcb_exit(i);
						socket_cerrar(i);
						FD_CLR(i, &master); // Elimina al socket del conjunto maestro
						destruir_men_comun(men_cod_prog);
						break;
					}

					printf("PLP-select: nuevo prog con socket n°%i\n", i);
					contador_prog++;

					//Pide mem para el prog
					t_resp_sol_mem *resp_sol = solicitar_mem(men_cod_prog->dato, param_plp->tam_stack,contador_prog);

					if (resp_sol->memoria_insuficiente == MEM_OVERLOAD){

						//Avisa al programa que no hay memoria
						t_men_comun *men_no_hay_mem = malloc(sizeof(t_men_comun));
						men_no_hay_mem->tipo = MEM_OVERLOAD;
						men_no_hay_mem->dato = string_itoa(contador_prog);
						socket_send_comun(i, men_no_hay_mem);
						destruir_men_comun(men_no_hay_mem);
						free(resp_sol);

					}else{

						//Crea el pcb y lo pone en new
						t_pcb_otros *pcb_otros = malloc(sizeof(t_pcb_otros));
						pcb_otros->n_socket = i;
						pcb_otros->pcb = crear_pcb_escribir_seg_UMV(men_cod_prog,resp_sol, contador_prog);
						pcb_otros->peso = calcular_peso(men_cod_prog);
						pcb_otros->tipo_fin_ejecucion = -1;
						pthread_mutex_lock(&mutex_new);
						queue_push(colas->cola_new,pcb_otros);
						pthread_mutex_unlock(&mutex_new);
						pthread_mutex_unlock(&mutex_miltiprog);
					}
					destruir_men_comun(men_cod_prog);
				}
			}
		}
	sleep(param_plp->retardo);
	}
	return NULL;
}

void *pcp(t_param_pcp *param_pcp){
	int32_t i, fdmax, cpu_new_fd; //lo mismo con int
	fd_set master, read_fds;

	// Crea el hilo que pasa los pcb de ready a exec
	pthread_t hilo_ready_exec;
	t_param_ready_exec *param_ready_exec = malloc(sizeof(t_param_ready_exec));
	param_ready_exec->quantum = param_pcp->quantum;
	pthread_create(&hilo_ready_exec, NULL, manejador_ready_exec, (void *)param_ready_exec);

	// Crea el hilo que maneja las llegadas de pcbs a exit
	pthread_t hilo_exit;
	pthread_create(&hilo_exit, NULL, manejador_exit, NULL);

	int32_t listener_cpu = socket_crear_server(param_pcp->puerto_cpu);
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	FD_SET(listener_cpu, &master);
	fdmax = listener_cpu;
	while(quit_sistema){
		read_fds = master;
		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
			perror("PCP-select");
			exit(1);
		}
		for(i = 3; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) {
				if (i == listener_cpu) { // Si los datos son en el srv que escucha cpus:
					cpu_new_fd = socket_accept(listener_cpu);
					handshake_cpu(cpu_new_fd);
					t_cpu *cpu = malloc(sizeof(t_cpu));

					// Arma estructura con el n° de socket del cpu y el id del prog que esta ejecutando
					cpu->soc_cpu = cpu_new_fd;
					cpu->id_prog_exec = 0;

					// Pone la estructura en la cola de cpus (cola con cpus libres)
					pthread_mutex_lock(&mutex_uso_cola_cpu);
					queue_push(cola_cpu, cpu);
					pthread_mutex_unlock(&mutex_uso_cola_cpu);
					pthread_mutex_unlock(&mutex_cola_cpu_vacia);
					printf("PCP-select: CPU conectada n°socket %i\n", cpu_new_fd);
					FD_SET(cpu_new_fd, &master);
					if (cpu_new_fd > fdmax)
						fdmax = cpu_new_fd;
				}else{
					// Sino, es un cpu mandando datos
					t_men_comun *men_cpu = socket_recv_comun(i);
					if (men_cpu->tipo == CONEC_CERRADA) {
						printf("PCP-select: CPU desconectada n°socket %i\n", i);

						// Agarra el cpu a partir de su n° de socket
						t_cpu *aux_cpu = get_cpu(i);

						if (aux_cpu->id_prog_exec != 0){

							// Saca el pcb que corresponde de la cola de ejec a partir del id
							t_pcb_otros *aux_pcb_otros = get_pcb_otros_exec(aux_cpu->id_prog_exec);
							t_men_comun *men = crear_men_comun(CPU_DESCONEC, "", 1);

							// Le manda msj al prog y pone el pcb en exit
							socket_send_comun(aux_pcb_otros->n_socket, men);
							pthread_mutex_lock(&mutex_exit);
							queue_push(colas->cola_exit, aux_pcb_otros);
							sem_incre(&cont_exit);
							pthread_mutex_unlock(&mutex_exit);
							destruir_men_comun(men);
						}
						free(aux_cpu);
						socket_cerrar(i);
						FD_CLR(i, &master);
						destruir_men_comun(men_cpu);
						continue;
					}
					if (men_cpu->tipo == FIN_QUANTUM){
						t_cpu *aux_cpu = get_cpu(i);
						if (aux_cpu->id_prog_exec != 0){

							// Actualizacion pcb
							t_pcb_otros *aux_pcb_otros=get_pcb_otros_exec(aux_cpu->id_prog_exec);
							t_pcb *pcb_recibido = socket_recv_quantum_pcb(i)->pcb;
							actualizar_pcb(aux_pcb_otros,pcb_recibido);

							// Setea en la estructura del cpu como que no esta ejecutando nada
							aux_cpu->id_prog_exec = 0;

							// Pone el pcb en ready
							pthread_mutex_lock(&mutex_ready);
							queue_push(colas->cola_ready,aux_pcb_otros);
							pthread_mutex_unlock(&mutex_ready);
							pthread_mutex_unlock(&mutex_ready_vacia);

							// Pone el cpu devuelta en la cola de cpus
							pthread_mutex_lock(&mutex_uso_cola_cpu);
							queue_push(cola_cpu,aux_cpu);
							pthread_mutex_unlock(&mutex_uso_cola_cpu);
							pthread_mutex_unlock(&mutex_cola_cpu_vacia);
						}
						destruir_men_comun(men_cpu);
						continue;
					}
					if(men_cpu->tipo == FIN_EJECUCION){
						t_cpu *aux_cpu = get_cpu(i);
						//Actualizar pcb?
						t_pcb_otros *aux_pcb_otros = get_pcb_otros_exec(aux_cpu->id_prog_exec);
						aux_pcb_otros->tipo_fin_ejecucion = FIN_EJECUCION;
						pthread_mutex_lock(&mutex_exit);
						queue_push(colas->cola_exit,aux_pcb_otros);
						pthread_mutex_unlock(&mutex_exit);

						sem_incre(&cont_exit);

						aux_cpu->id_prog_exec = 0;

						pthread_mutex_lock(&mutex_uso_cola_cpu);
						queue_push(cola_cpu,aux_cpu);
						pthread_mutex_unlock(&mutex_uso_cola_cpu);
						pthread_mutex_unlock(&mutex_cola_cpu_vacia);
						destruir_men_comun(men_cpu);
						continue;
					}
					/*
					 * Primero recibe un msj con el id de la llamada al sistema y el valor,
					 * dsp otro msj con el id del prog al que le quiere mandar eso
					 * Manda entonces un msj al prog para que imprima el dato.
					 */
					if ((men_cpu->tipo == IMPRIMIR_TEXTO) || (men_cpu->tipo == IMPRIMIR_VALOR)) {
						t_men_comun *aux_men_cpu = socket_recv_comun(i);
						if (aux_men_cpu->tipo != ID_PROG)
							printf("Se esperaba el tipo de dato %i y se obtuvo %i\n",ID_PROG,men_cpu->tipo);
						t_pcb_otros *aux_pcb_otros;
						aux_pcb_otros = get_pcb_otros_exec_sin_quitarlo(atoi(aux_men_cpu->dato));
						socket_send_comun(aux_pcb_otros->n_socket, aux_men_cpu);
						//todo habria que mandarle un msj a la cpu de que termino la llamada asi sigue procesando, seria mejor
						destruir_men_comun(aux_men_cpu); //cuidado, quizas destruye los datos antes que el programa los reciba etc
						continue;
					}
					if(men_cpu->tipo == OBTENER_VALOR){
						char *valor	= dictionary_get(diccionario_variables,men_cpu->dato);
						men_cpu->tipo = VALOR_ASIGNADO; //todo poner otro id para cuando devuelvo el valor, ver con flor
						men_cpu->dato = valor;
						socket_send_comun(i,men_cpu);
						destruir_men_comun(men_cpu);
						continue;
					}
					if(men_cpu->tipo == GRABAR_VALOR){
						t_men_comun *aux_men_cpu = socket_recv_comun(i);
						if((aux_men_cpu->tipo) == VALOR_ASIGNADO)
							dictionary_put(diccionario_variables,men_cpu->dato,aux_men_cpu->dato); //todo pregunar si se reemplaza el dato o que
						destruir_men_comun(men_cpu);
						destruir_men_comun(aux_men_cpu);
						//todo porque flor espera un msj aca
						continue;
					}
					if (men_cpu->tipo == IO_ID){
						enviar_IO(i, atoi(men_cpu->dato));
						continue;
					}

				}
			}
		}
	sleep(param_pcp->retardo);
	}

	return NULL;
}

t_pcb_otros *get_pcb_otros_exec_sin_quitarlo(int32_t id_proc){
	t_pcb_otros *aux_pcb_otros;
	int32_t i;

	pthread_mutex_lock(&mutex_exec);
	int32_t tam = queue_size(colas->cola_exec);
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
	pthread_mutex_unlock(&mutex_exec);
	printf("ERROR EN get_pcb_otros_exec\n");
	return NULL;
}

int32_t mover_pcb_exit(int32_t soc_prog){
	int32_t i;
	t_pcb_otros *aux;

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

t_cpu *get_cpu(int32_t soc_cpu){
	int32_t i;
	t_cpu *cpu;
	pthread_mutex_lock(&mutex_uso_cola_cpu);
	int32_t tam = queue_size(cola_cpu);

	for(i=0 ;i < tam ;i++){
		cpu = queue_pop(cola_cpu);
		if (cpu->soc_cpu == soc_cpu){
			pthread_mutex_unlock(&mutex_uso_cola_cpu);
			return cpu;
		}
		queue_push(cola_cpu, cpu);
	}
	pthread_mutex_unlock(&mutex_uso_cola_cpu);
	printf("ERROR EN GET_CPU\n");
	return NULL;
}

t_pcb_otros *get_pcb_otros_exec(int32_t id_proc){
	t_pcb_otros *aux_pcb_otros;
	int32_t i;
	int32_t tam = queue_size(colas->cola_exec);

	pthread_mutex_lock(&mutex_exec);
	for(i=0 ;i < tam ;i++){
		aux_pcb_otros = queue_pop(colas->cola_exec);
		if (id_proc == aux_pcb_otros->pcb->id){
			pthread_mutex_unlock(&mutex_exec);
			return aux_pcb_otros;
		}
		queue_push(colas->cola_exec, aux_pcb_otros);
	}
	pthread_mutex_unlock(&mutex_exec);
	printf("ERROR EN get_pcb_otros_exec\n");
	return NULL;
}

void enviar_IO(int32_t soc_cpu, int32_t id_IO){
	t_men_comun *men;
	t_IO *aux_IO;
	t_cpu *aux_cpu;
	aux_cpu = get_cpu(soc_cpu);

	int32_t i;

	//Busca el dispositivo de IO en la lista y lo guarda en aux_IO
	pthread_mutex_lock(&mutex_dispositivos_io);
	for (i=0; i < list_size(dispositivos_IO); i++){
		aux_IO = list_remove(dispositivos_IO,i);
		if(atoi(aux_IO->id_hio)==id_IO){
			i=list_size(dispositivos_IO);
		}
		else{
			list_add(dispositivos_IO,aux_IO);
		}
	}
	pthread_mutex_unlock(&mutex_dispositivos_io);

	men = socket_recv_comun(soc_cpu);
	if (men->tipo != IO_CANT_UNIDADES)
		printf("ERROR: La CPU mando el tipo %i y se esperaba %i\n",men->tipo,IO_CANT_UNIDADES); //si hay un error, no deberia terminar?
	int32_t cant_unidades = atoi(men->dato);

	//Crea la estructura que tiene el id del programa y la cantidad de unidades que quiere usar para ponerlo en la cola del dispositivo
	t_IO_espera *espera = malloc(sizeof(t_IO_espera));
	espera->id_prog = (aux_cpu->id_prog_exec);
	espera->unidades = cant_unidades;
	queue_push(aux_IO->procesos,espera);

	//Recibo el pcb del cpu para actualizarlo y lo saco de la cola de ejecucion todo correguir tmb actualizacion
	t_pcb_otros *aux_pcb_otros=get_pcb_otros_exec(aux_cpu->id_prog_exec);
	aux_pcb_otros->pcb = (socket_recv_quantum_pcb(soc_cpu)->pcb);
	aux_cpu->id_prog_exec = 0;

	pthread_mutex_lock(&mutex_uso_cola_cpu);
	queue_push(cola_cpu,aux_cpu);
	pthread_mutex_unlock(&mutex_uso_cola_cpu);
	pthread_mutex_unlock(&mutex_cola_cpu_vacia);

	//Pongo el pcb en bloqueados
	pthread_mutex_lock(&mutex_block);
	queue_push(colas->cola_block,aux_pcb_otros);
	pthread_mutex_unlock(&mutex_block);
	destruir_men_comun(men);
}

t_pcb_otros *get_peso_min(){
	int32_t i, peso_min = -1;
	t_pcb_otros *aux;

	int32_t tam_cola = queue_size(colas->cola_new);

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

void *manejador_exit(){ //todo dudas en general
	t_pcb_otros *aux_pcb_otros;
	int32_t soc_prog;

	while(quit_sistema){
		pthread_mutex_lock(&mutex_exit);
		if (queue_is_empty(colas->cola_exit)){ //aca no hay espera activa??
			pthread_mutex_unlock(&mutex_exit);
			sem_decre(&cont_exit); //por que se decrementa aca y no en el else? No se deberia incrementar aca?
		}else{
			aux_pcb_otros = queue_pop(colas->cola_exit);
			pthread_mutex_unlock(&mutex_exit);
			if (aux_pcb_otros->tipo_fin_ejecucion == CPU_DESCONEC ||aux_pcb_otros->tipo_fin_ejecucion == FIN_EJECUCION ){
				t_men_comun *men;
				men = crear_men_comun(aux_pcb_otros->tipo_fin_ejecucion,"",1);
				soc_prog = aux_pcb_otros->n_socket;
				socket_send_comun(soc_prog , men);
			}
			umv_destrui_pcb(aux_pcb_otros->pcb->id);
			free(aux_pcb_otros->pcb); //sincronizar, que la umv mande msj cuando termino de destruir los segmentos y recien ahi destruir el pcb
			free(aux_pcb_otros);
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
			sem_decre(&libre_multiprog); //no entiendo por qué se decrementa aca
			pthread_mutex_lock(&mutex_new);
			pthread_mutex_lock(&mutex_ready);
			queue_push(colas->cola_ready, get_peso_min());
			pthread_mutex_unlock(&mutex_new);
			pthread_mutex_unlock(&mutex_ready);
			pthread_mutex_unlock(&mutex_ready_vacia);
			sem_incre(&buff_multiprog);
		}
	}
	free(param);
	return NULL;
}

void *manejador_ready_exec(t_param_ready_exec *param){
	t_pcb_otros *aux_pcb_otros;
	t_cpu *cpu;
	int32_t res;
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
				//todo ver si el quantum siempre es el mismo, sino habria que agregarlo a pcb_otros
				//para que se actualice cuando llega desde el pcb
				socket_send_pcb(cpu->soc_cpu,aux_pcb_otros->pcb,param->quantum);
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

t_cpu *get_cpu_libre(int32_t *res){
	int32_t i;
	t_cpu *cpu;

	int32_t tam = queue_size(cola_cpu);
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

void umv_destrui_pcb(int32_t id_pcb){ //ojo que hay que sincronizar los mensajes con la umv

	t_men_seg *men_seg;
	men_seg = crear_men_seg(DESTR_SEGS, id_pcb, 0);
	socket_send_seg(soc_umv, men_seg);
	destruir_men_seg(men_seg);
}

t_pcb *crear_pcb_escribir_seg_UMV(t_men_comun *men_cod_prog ,t_resp_sol_mem *resp_sol ,int32_t contador_id_programa){

	//ojo que hay que sincronizar los mensajes con la umv

	t_metadata_program *metadata_program = metadata_desde_literal(men_cod_prog->dato);
	t_men_comun* men;
	char *etis = metadata_program->etiquetas;
	//TODO puse 205 porque ESCRIBIR_SEG no lo toma
	// Escribe el segmento de codigo
	socket_send_seg(soc_umv,crear_men_seg(205, contador_id_programa, 0));
	men = crear_men_comun(CODIGO_SCRIPT,men_cod_prog->dato,men_cod_prog->tam_dato);
	socket_send_comun(soc_umv, men);

	// Escribe el segmento de indice de etiquetas
	socket_send_seg(soc_umv,crear_men_seg(205, contador_id_programa, 0));
	men = crear_men_comun(IND_ETI_FUNC,etis,metadata_program->etiquetas_size);
	socket_send_comun(soc_umv, men);

	// Escribe el segmento de indice de codigo
	socket_send_seg(soc_umv,crear_men_seg(205, contador_id_programa, 0));
	int32_t tam_ind_cod = metadata_program->instrucciones_size*8;
	men = crear_men_comun(IND_COD,(void *)metadata_program->instrucciones_serializado,tam_ind_cod);
	socket_send_comun(soc_umv, men_cod_prog);

	t_pcb *pcb = malloc(sizeof(t_pcb));
	pcb->id = contador_id_programa;
	pcb->dir_primer_byte_umv_segmento_codigo = resp_sol->dir_primer_byte_umv_segmento_codigo;
	pcb->dir_primer_byte_umv_segmento_stack = resp_sol->dir_primer_byte_umv_segmento_stack;
	pcb->dir_primer_byte_umv_contexto_actual = resp_sol->dir_primer_byte_umv_segmento_stack;
	pcb->dir_primer_byte_umv_indice_codigo = resp_sol->dir_primer_byte_umv_indice_codigo;
	pcb->dir_primer_byte_umv_indice_etiquetas = resp_sol->dir_primer_byte_umv_indice_etiquetas;
	pcb->program_counter = metadata_program->instruccion_inicio;
	pcb->cant_var_contexto_actual = 0;
	pcb->tam_indice_etiquetas = metadata_program->etiquetas_size;

	destruir_men_comun(men);
	return pcb;
}

t_resp_sol_mem * solicitar_mem(char *script, int32_t tam_stack, int32_t id_prog){ //ojo que hay que sincronizar los msj con la umv

	t_resp_sol_mem *resp_sol = malloc(sizeof(t_resp_sol_mem));
	resp_sol->memoria_insuficiente = 0;
	t_men_comun *resp_mem;
	t_men_seg *ped_mem;
	int32_t tam = 0;

	//pido mem para el codigo del script
	ped_mem = crear_men_seg(PED_MEM_SEG_COD,id_prog,string_length(script));
	socket_send_seg(soc_umv, ped_mem);
	t_men_comun *resp_men_cod = socket_recv_comun(soc_umv);

	if (resp_men_cod->tipo == MEM_OVERLOAD){
		resp_sol->memoria_insuficiente = MEM_OVERLOAD;
		destruir_men_comun(resp_men_cod);
		destruir_men_seg(ped_mem);
		return resp_sol;
	}

	if( resp_men_cod->tipo != RESP_MEM_SEG_COD)
		printf("ERROR: se esperaba %i y obtube un %i\n",RESP_MEM_SEG_COD,resp_men_cod->tipo); //por que no termina si hay un error??

	t_dir_mem dir_mem_cod = atoi(resp_men_cod->dato);
	resp_sol->dir_primer_byte_umv_segmento_codigo = dir_mem_cod;

	//pido mem para el indice de etiquetas y funciones
	t_metadata_program* metadata_program = metadata_desde_literal(script);
	tam = (metadata_program->etiquetas_size);
	ped_mem = crear_men_seg( PED_MEM_IND_ETI , id_prog, tam);
	socket_send_seg(soc_umv, ped_mem);
	resp_mem = socket_recv_comun(soc_umv);
	if (resp_mem->tipo == MEM_OVERLOAD){
		resp_sol->memoria_insuficiente = MEM_OVERLOAD;
		destruir_men_comun(resp_mem);
		destruir_men_seg(ped_mem);
		return resp_sol;
	}

	if( resp_mem->tipo != RESP_MEM_IND_ETI)
		printf("ERROR: se esperaba %i y obtube un %i\n",RESP_MEM_IND_ETI,resp_mem->tipo);

	resp_sol->dir_primer_byte_umv_indice_etiquetas= atoi(resp_mem->dato);

	//pido mem para el indice de codigo
	tam = (metadata_program->instrucciones_size*8);
	ped_mem = crear_men_seg( PED_MEM_IND_COD , id_prog, tam);
	socket_send_seg(soc_umv, ped_mem);
	resp_mem = socket_recv_comun(soc_umv);

	if (resp_mem->tipo == MEM_OVERLOAD){
		resp_sol->memoria_insuficiente = MEM_OVERLOAD;
		destruir_men_comun(resp_mem);
		destruir_men_seg(ped_mem);
		return resp_sol;
	}

	if( resp_mem->tipo != RESP_MEM_IND_COD)
		printf("ERROR: se esperaba %i y obtube un %i\n",RESP_MEM_IND_COD,resp_mem->tipo);

	resp_sol->dir_primer_byte_umv_indice_codigo = atoi(resp_mem->dato);

	//pido mem para el stack
	ped_mem = crear_men_seg( PED_MEM_SEG_STACK , id_prog, tam_stack);
	socket_send_seg(soc_umv, ped_mem);
	resp_mem = socket_recv_comun(soc_umv);

	if (resp_mem->tipo == MEM_OVERLOAD){
		resp_sol->memoria_insuficiente = MEM_OVERLOAD;
		destruir_men_comun(resp_mem);
		destruir_men_seg(ped_mem);
		return resp_sol;
	}

	if( resp_mem->tipo != RESP_MEM_SEG_STACK)
		printf("ERROR: se esperaba %i y obtube un %i\n",RESP_MEM_SEG_STACK,resp_mem->tipo);

	t_dir_mem dir_mem_stack = atoi(resp_mem->dato);
	resp_sol->dir_primer_byte_umv_segmento_stack = dir_mem_stack;
	destruir_men_comun(resp_mem);
	destruir_men_seg(ped_mem);
	return resp_sol;
}

int32_t calcular_peso(t_men_comun *men_cod_prog){
	t_metadata_program *metadata_program = metadata_desde_literal(men_cod_prog->dato);
	return (5 * (metadata_program->cantidad_de_etiquetas) + 3 * (metadata_program->cantidad_de_funciones) + (metadata_program->instrucciones_size));
}

void handshake_umv(char *ip_umv, char *puerto_umv){ //sincronizar msjs con la umv

	//envio a la UMV
	soc_umv = socket_crear_client(puerto_umv,ip_umv);
	t_men_comun *mensaje_inicial = crear_men_comun(HS_KERNEL,"",string_length(""));
	socket_send_comun(soc_umv,mensaje_inicial);

	//espero coneccion de la UMV
	mensaje_inicial = socket_recv_comun(soc_umv);

	if(mensaje_inicial->tipo == HS_UMV){
		printf("UMV conectada\n");
	}else{
		printf("ERROR HANDSHAKE");
	}
	destruir_men_comun(mensaje_inicial);
}

void manejador_IO(t_IO *io){
	//Sincronizar acceso a la cola
	while(quit_sistema){
		if(queue_size(io->procesos)==0)
			pthread_mutex_lock(&(io->mutex_dispositivo));
		else{
		t_IO_espera *proceso = queue_pop(io->procesos);
		int32_t tamanio_cola_block = queue_size(colas->cola_block);
		int32_t i;

		for(i=0; i<proceso->unidades;i++)
			sleep(io->hio_sleep);

		pthread_mutex_lock(&mutex_block);

		for (i=0; i<tamanio_cola_block; i++){
			t_pcb_otros *pcb_aux;
			pcb_aux =queue_pop(colas->cola_block);

			if (pcb_aux->pcb->id==proceso->id_prog){
				pthread_mutex_lock(&mutex_ready);
				queue_push(colas->cola_ready, pcb_aux);
				pthread_mutex_unlock(&mutex_ready);
				pthread_mutex_unlock(&mutex_ready_vacia);
				i=tamanio_cola_block;
			}

			else {
				queue_push(colas->cola_block,pcb_aux);
			}
		}
		pthread_mutex_unlock(&mutex_block);
		pthread_mutex_unlock(&(io->mutex_dispositivo));
		}
	}
}

void socket_send_pcb(int32_t soc,t_pcb *pcb,int32_t quantum){
	socket_send_quantum_pcb(soc,crear_men_quantum_pcb(PCB_Y_QUANTUM,quantum,pcb));
}

void actualizar_pcb(t_pcb_otros *pcb, t_pcb *pcb_actualizado){

}

t_datos_config *levantar_config(){

	int i;
	t_IO *aux_IO;
	char **id_hios = malloc(sizeof(char));
	char **hio_sleeps = malloc(sizeof(char));
	t_config *diccionario_config = config_create("kernel_config");
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

		new_IO->procesos = queue_create();
		queue_push(ret->cola_IO, new_IO);
		pthread_create(&(new_IO->hilo), NULL, (void*)manejador_IO, (void*)new_IO);
	}

	ret->semaforos = config_get_array_value( diccionario_config, "SEMAFOROS");
	ret->valor_semaforos = config_get_array_value( diccionario_config, "VALOR_SEMAFORO");
	ret->multiprogramacion = config_get_int_value( diccionario_config, "MULTIPROGRAMACION");
	ret->quantum = config_get_int_value( diccionario_config, "QUANTUM");
	ret->retardo = config_get_int_value( diccionario_config, "RETARDO");
	ret->tam_stack = config_get_int_value( diccionario_config, "TAMANIO_STACK");
	ret->variables_globales = config_get_array_value( diccionario_config, "VARIABLES_GLOBALES");

	for (i=0; ret->variables_globales[i] != '\0'; i++){
		dictionary_put(diccionario_variables,ret->variables_globales[i],NULL);
	}

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

void handshake_cpu(int32_t soc){ //todo hay que sincronizar los mensajes con los cpus? Y con los programas??

	t_men_comun *handshake;
	handshake = socket_recv_comun(soc);

	if(handshake->tipo != HS_CPU)
		printf("ERROR HANDSHAKE CPU = %i\n",handshake->tipo);

	handshake->tipo=HS_KERNEL;
	socket_send_comun(soc,handshake);

	destruir_men_comun(handshake);
}

void handshake_prog(int32_t soc){

	t_men_comun *men_hs;
	men_hs = socket_recv_comun(soc);

	if(men_hs->tipo != HS_PROG)
		printf("ERROR se esperaba HS_PROG y se recibio %i\n",men_hs->tipo);

	men_hs->tipo = HS_KERNEL;
	socket_send_comun(soc, men_hs);

	destruir_men_comun(men_hs);
}

void *imp_colas (){

	int32_t i;
	t_pcb_otros *aux;

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

void crear_cont(sem_t *sem ,int32_t val_ini){
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
