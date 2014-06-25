#include "UMV.h"
int32_t quit_sistema = 1;
char *mem_prin;
int32_t retardo = 0;
char alg_actual;
char *proc_activo = "0";
t_dictionary *tablaProgramas;
t_list *listaAuxiliar;
t_config *ptrConfig;
pthread_mutex_t mutex_mem_prin = PTHREAD_MUTEX_INITIALIZER;

int32_t main(){
	srand(time(0));
	//crear configuracion y solicitar memoria
	ptrConfig = config_create("./UMV/umv_config.txt");
	encabezado(config_get_int_value(ptrConfig,"tamanio"),config_get_string_value(ptrConfig,"modo"));
	mem_prin = malloc(config_get_int_value(ptrConfig,"tamanio"));
	tablaProgramas = dictionary_create();
	alg_actual = config_get_string_value(ptrConfig,"modo")[0];

	pthread_t hilo_consola;
	pthread_t hilo_conecciones;
	pthread_create( &hilo_conecciones, NULL, admin_conecciones,NULL);
	pthread_create( &hilo_consola, NULL, crearConsola, NULL);
	pthread_join( hilo_conecciones, NULL);
	pthread_join( hilo_consola, NULL);

	return 0;
}

void *admin_conecciones(){
	int32_t listen_soc = socket_crear_server(config_get_string_value(ptrConfig,"puerto"));
	int32_t new_soc;
	t_men_comun *men_hs;
	pthread_t hilo_conec_kernel;
	t_param_conec_kernel *param_kernel = malloc(sizeof(t_param_conec_kernel));
	while(quit_sistema){
		new_soc = socket_accept(listen_soc);
		men_hs = socket_recv_comun(new_soc);
		if (men_hs->tipo == CONEC_CERRADA){
			printf("Socket nº%i desconectado",new_soc);
			continue;
		}
		if (men_hs->tipo >= HS_KERNEL){//si se conecta el kernel
			param_kernel->soc = new_soc;
			pthread_create( &hilo_conec_kernel, NULL, admin_conec_kernel,param_kernel);
			continue;
		}
		if (men_hs->tipo >= HS_CPU){//si es una cpu nueva
			//todo crear hilo para administrar la cpu nueva
			continue;
		}
		printf("ERROR se esperaba recibir un tipo handshake y se recibio %i", men_hs->tipo);
	}
	socket_cerrar(listen_soc);
	return NULL;
}

void *admin_conec_kernel(t_param_conec_kernel *param){
	t_men_comun *aux_men;
	t_men_seg *men_seg;

	t_men_comun *men_hs= crear_men_comun(HS_UMV,NULL,0);
	socket_send_comun(param->soc,men_hs);

	while(quit_sistema){
		men_seg = socket_recv_seg(param->soc);
		if (men_seg->tipo == DESTR_SEGS){
			destruirSegmentos(string_itoa(men_seg->id_prog));
			continue;
		}
		if (crearSegmento(men_seg) == -1){
			destruirSegmentos(string_itoa(men_seg->id_prog));
			aux_men= crear_men_comun(MEM_OVERLOAD,NULL,0);
			socket_send_comun(param->soc,aux_men);
		}else{
			switch (men_seg->tipo) {
			case PED_MEM_SEG_COD:
				aux_men= crear_men_comun(RESP_MEM_SEG_COD,NULL,0);
				break;
			case PED_MEM_IND_ETI:
				aux_men= crear_men_comun(RESP_MEM_IND_ETI,NULL,0);
				break;
			case PED_MEM_IND_COD:
				aux_men= crear_men_comun(RESP_MEM_IND_COD,NULL,0);
				break;
			case PED_MEM_SEG_STACK:
				aux_men= crear_men_comun(RESP_MEM_SEG_STACK,NULL,0);
				break;
			}
			socket_send_comun(param->soc,men_hs);
		}
		sleep(retardo);
	}
	return NULL;
}

void *admin_conec_cpu(t_param_conec_cpu *param){
	while(quit_sistema){
		t_men_comun *men_hs= crear_men_comun(HS_UMV,NULL,0);
		socket_send_comun(param->soc,men_hs);

		sleep(retardo);
	}
	return NULL;
}

bool compararListaAuxiliar(ListAuxiliar *nodo1, ListAuxiliar *nodo2){
	return ((nodo1->ptrInicio) < (nodo2->ptrInicio));
}

void compactar(){
	ListAuxiliar *p1 = malloc(sizeof(ListAuxiliar));
	ListAuxiliar *p2 = malloc(sizeof(ListAuxiliar));
	int32_t x;
	TabMen *actualizar;
	void *aux1;
	void *aux2;
	/*recorrer tabla de programas y crear una lista auxiliar ordenada, preguntar si hay espacio
	entre dos nodos, si hay se desplaza la memoria y se actualiza la tabla de programas*/
	recorrerTablaSegmentos();
	list_sort(listaAuxiliar, (void*)compararListaAuxiliar);
	for (x=0;x<list_size(listaAuxiliar)-1;x++){
		p1 = list_get(listaAuxiliar,x);
		p2 = list_get(listaAuxiliar,x+1);

		if (p2->ptrInicio != (p1->ptrFin) +1){
			int32_t aux = (p1->ptrFin)+1;
			aux1 = (&mem_prin + aux);
			aux = p2->ptrInicio;
			aux2 = (&mem_prin + aux);
			memcpy(aux1, aux2,(p2->ptrFin - p2->ptrInicio));
			actualizar = p2->ptrATabla;
			actualizar->memFisica = p1->ptrFin+1;
			p2->ptrFin = p1->ptrFin+1 + (p2->ptrFin - p2->ptrInicio);
			p2->ptrInicio = p1->ptrFin+1;

		}
	}
	list_destroy(listaAuxiliar);
}

int32_t crearSegmento(t_men_seg *men_ped){
	void *lista;
	int32_t memEstaOk;
	int32_t tamanio = men_ped->tam_seg;
	char *id_Prog = string_itoa(men_ped->id_prog);

	/*pregunta si hay memoria, si no la hay retorna -1*/
	int32_t aux_mem_fisica = asignarMemoria(tamanio);
	if (aux_mem_fisica == -1)
		return -1;

	TabMen *nodoTab = malloc(sizeof(TabMen));
	/*pregunta si en la tabla de programas existe el id de prog,
		 si no existe se agrega el key al diccionario y si existe
		 se agrega un nodo a la lista de segmentos, controlando que
		 la memoria logica no se pise dentro del mismo programa */
	nodoTab->tipo_seg = men_ped->tipo;
	nodoTab->longitud = tamanio;
	nodoTab->memFisica = aux_mem_fisica;

	if (dictionary_has_key(tablaProgramas, id_Prog)){
		lista = dictionary_get(tablaProgramas,id_Prog);
			/*controla que no se pise la memoria dentro del mismo programa
			repitiendo el ciclio hasta que se genere uno valido*/
			do {
			memEstaOk = 1;
			nodoTab->memLogica = asignarMemoriaAleatoria(tamanio);
			memEstaOk = controlarMemPisada(lista,nodoTab->memLogica,tamanio);
			}while (!memEstaOk);
		list_add(lista,nodoTab);
	}else{
		nodoTab->memLogica = asignarMemoriaAleatoria(tamanio);
		lista = list_create();
		list_add(lista,nodoTab);
		dictionary_put(tablaProgramas, id_Prog, lista);
	}
	return 0;
}

int32_t controlarMemPisada(void *lista, int32_t numMemoria, int32_t tamanio){
	int32_t x;
	TabMen *nodo;
	for (x=0;list_size(lista)-1;x++){
		nodo = list_get(lista,x);
		if ((((nodo->memLogica) <= numMemoria) && ((nodo->memLogica + nodo->longitud) >= numMemoria)) || (((nodo->memLogica) <= numMemoria+tamanio) && ((nodo->memLogica + nodo->longitud) >= numMemoria+tamanio)) || ((numMemoria <= nodo->memLogica) && (numMemoria >= (nodo->memLogica+nodo->longitud)))){
				return 0;
		}
	}
	return 1;
}

int32_t asignarMemoria(int32_t tamanio){
	ListAuxiliar *p1 = malloc(sizeof(ListAuxiliar));
	ListAuxiliar *p2 = malloc(sizeof(ListAuxiliar));
	int32_t x;
	int32_t espacioDisponible;
	int32_t hayEspacio;
	int32_t seCompacto;
	struct peorEspacio {
		int32_t espacio;
		int32_t offset;
	};

	struct peorEspacio pE;

	seCompacto =0;
	hayEspacio = 0;
	/*se recorre la tabla de segmentos y se crea una lista auxiliar
	 ordenada segun memoria fisica. Se comparan dos nodos, si hay
	 espacio entre ellos, se pregunta si ese espacio es suficiente, si
	 lo es se asigna segun el algoritmo en uso. Si no hay espacio se
	 compacta y se vuelve a buscar lugar. */
	recorrerTablaSegmentos();
	list_sort(listaAuxiliar, (void *)compararListaAuxiliar);

	etiqueta_1:
	for (x=0;x<list_size(listaAuxiliar)-1;x++){
		p1 = list_get(listaAuxiliar,x);
		p2 = list_get(listaAuxiliar,x+1);

		if (p2->ptrInicio != (p1->ptrFin) +1){
			espacioDisponible = (p2->ptrInicio - p1->ptrFin -1);
			if (espacioDisponible >= tamanio){
				hayEspacio=1;
				switch (alg_actual){
					case'F':
						return (p1->ptrFin+1);
						break;
					case 'W':
						if (espacioDisponible > pE.espacio){
							pE.espacio = espacioDisponible;
							pE.offset = p1->ptrFin+1;
						}
						break;
				}
			}
				if (hayEspacio)
					return pE.offset;
				if (!hayEspacio && !seCompacto){
					seCompacto = 1;
					compactar();
					goto etiqueta_1;
				}
				if (!hayEspacio && seCompacto)
					return -1;
		}
	}
	return 0;
}

int32_t asignarMemoriaAleatoria(int32_t tamanio){
	int32_t numero;
	numero = (rand());
	return numero;
}

void recorrerTablaSegmentos(){
	listaAuxiliar= list_create();
	dictionary_iterator(tablaProgramas,recorrerLista);
	insertarNodosBarrera();
}

void recorrerLista(char* clave, void *ptrLista){
	list_iterate(ptrLista,(void *)completarListaAuxiliar);
}

void completarListaAuxiliar(TabMen *nodo){
	ListAuxiliar *nodoAux = malloc(sizeof(ListAuxiliar));

	nodoAux->ptrInicio = nodo->memFisica;
	nodoAux->ptrFin = nodo->memFisica + nodo->longitud;
	nodoAux->ptrATabla = nodo;

	list_add(listaAuxiliar, nodoAux);
}
//en la lista auxiliar inserta un nodo ficticio al inicio y al final para poder hacer las comparaciones con el primer y ultimo nodo real
void insertarNodosBarrera (){
	ListAuxiliar *nodoAux = malloc(sizeof(ListAuxiliar));
	nodoAux->ptrInicio = -1;
	nodoAux->ptrFin = -1;
	list_add_in_index(listaAuxiliar,0,nodoAux);
	nodoAux->ptrInicio=config_get_int_value(ptrConfig,"tamanio");
	nodoAux->ptrFin = config_get_int_value(ptrConfig,"tamanio");
	list_add(listaAuxiliar,nodoAux);
}

void destruirSegmentos(char *id_Prog){
	void *lista;
	lista = dictionary_get(tablaProgramas, id_Prog);
	list_destroy_and_destroy_elements(lista, (void *)eliminarElemento);
}

void eliminarElemento(void *elemento){
	free (elemento);
}

void *solicitarBytes (int32_t base, int32_t offset, int32_t tamanio){
	//se tiene el proceso activo
	void *lista;
	TabMen *segmento;
	char *b;
	b = malloc(tamanio);
	lista = dictionary_get(tablaProgramas, proc_activo);
	// encontrar el nodo(segmento) en donde coincide la base y la memoria logica
	segmento = encontrarSegmento (lista, base);
	// teniendo el segmento, controlar que no se excedan los limites
	if (base+offset > (segmento->memLogica+segmento->longitud) || base+offset+tamanio > (segmento->memLogica+segmento->longitud)){
		//TODO SEGMENTATION FAULT
	}
	else{
		void *origen;
		origen = mem_prin+segmento->memFisica+offset;
		memcpy (b,origen,tamanio);
		//hay q enviar b
	}
	return b;
}

TabMen *encontrarSegmento(void *lista, int32_t base){
	int32_t x;
	TabMen *nodo;
	for (x=0;list_size(lista)-1;x++){
		nodo = list_get(lista,x);
		if (nodo->memLogica == base){
			return nodo;
		}
	}
	printf("ERROR no se ha encontrado el segmento con la dit_logica %i",base);
	return NULL;
}

void almacenarBytes (int32_t base,int32_t offset,int32_t tamanio, char *buffer){
	//encontrar el segmento al q pertenece la base
	void *lista;
	TabMen *segmento;
	lista = dictionary_get(tablaProgramas, proc_activo);
	segmento = encontrarSegmento (lista,base);
	//controlar que no se excedan los limites
	if (base+offset > (segmento->memLogica+segmento->longitud) || base+offset+tamanio > (segmento->memLogica+segmento->longitud)){
		//SEGMENTATION FAULT
	}
	else{
		void *destino = mem_prin+segmento->memFisica+offset;
		memcpy(destino,buffer,tamanio);
	}
}
// error en iterator, en todas las funciones con orden superior

//consola
void* crearConsola(){
	char opcion;
	do {
		menuPrincipal();
		do {
			scanf("%c", &opcion);
		} while (opcion<'1' || opcion>'6');

		switch (opcion) {
			case '1':
				operacion();
				break;
			case '2':
				cambiarRetardo();
				break;
			case '3':
				cambiarAlgoritmo();
				break;
			case '4':
				compactar();
				break;
			case '5':
				imprimirDump();
				break;
			}
	}while (opcion != '6');
	return NULL;
}

void menuPrincipal(){
	printf ("--------------------------------\n"
			"Elija una opcion:\n"
			"	1.Operacion\n"
			"	2.Retardo\n"
			"	3.Cambiar Algoritmo\n"
			"	4.Compactacion\n"
			"	5.Dump\n"
			"	6.Salir\n"
			"--------------------------------\n");
}

void operacion(){
	int32_t base, offset, tam, id_proc;

	//todo creo que aca se puede elegir entre crear/destruir un segmento o solicitar/escribir una pos de memoria
	printf ("--------------------------------\n");
	printf("ID del proceso:");
	scanf("%i",&id_proc);
	printf("Base:");
	scanf("%i",&base);
	printf("Offset:");
	scanf("%i",&offset);
	printf("Tamaño:");
	scanf("%i",&tam);
	printf ("--------------------------------\n");
	//todo hacer operacion

}

void cambiarAlgoritmo(){
	printf ("--------------------------------\n");
	if(alg_actual == 'W'){
		alg_actual = 'F';
		printf("Algoritmo cambiado a = First-Fit\n");
	}else{
		alg_actual = 'W';
		printf("Algoritmo cambiado a = Worst-Fit\n");
	}
}

void cambiarRetardo(){
	int32_t t;

	printf ("--------------------------------\n");
	printf("Ingrese el tiempo:");
	if (scanf("%i",&t)!= EOF && t>0){
		retardo = t;
		printf("Nuevo retardo: %i\n", t);
	}else{
		printf("Tiempo no válido.\n");
	}

}

void imprimirDump(){
	char opcion;
	printf ("--------------------------------\n"
			"Elija lo que desea imprimir:\n"
			"	1.Estructuras de memoria\n"
			"	2.Memoria principal\n"
			"	3.Contenido memoria principal\n"
			"--------------------------------\n");
	do {
		scanf("%c", &opcion);
	} while (opcion<'1' || opcion>'3');

	switch (opcion) {
	case '1':
		imp_estructura_mem();
		break;
	case '2':
		imp_mem_prin();
		break;
	case '3':
		imp_cont_mem_prin();
		break;
	}
}

void imp_estructura_mem(){
	int id_prog;

	printf ("--------------------------------\n"
			"Elija el id_proceso\n"
			"Ingrese 0 si desea imprimir todos\n");
	scanf("%i",&id_prog);
	int32_t resp = imp_tablas_segmentos(id_prog);
	if (resp == -1)
		printf("El proceso no existe\n");
}

int32_t imp_tablas_segmentos(int32_t id_prog){
	//todo usar semaforos para la tabla de programas
	if (id_prog == 0){
		dictionary_iterator(tablaProgramas, (void *)imp_lista);
		return 1;
	}
	t_list *lista_seg_prog = dictionary_get(tablaProgramas, string_itoa(id_prog));
	if (lista_seg_prog == NULL) //todo probar que funke
		return -1;
	imp_lista(string_itoa(id_prog), lista_seg_prog);
	return 1;
}

void imp_lista(char *id_prog, t_list *lista_seg){
	int i, tam_list;
	TabMen *aux_tab_mem = malloc(sizeof(TabMen));
	printf("llegueACAAA\n");
	tam_list = list_size(lista_seg);
	if (tam_list == 0)
		printf("ERROR tabla de segmentos vacia\n");
	for (i=0; i<tam_list;i++){
		aux_tab_mem = list_get(lista_seg, i);
		printf("TIPO | ID  |DIR_F| TAM |DIR_L|\n");
		printf(" %i  |",aux_tab_mem->tipo_seg);
		printf(" %s  |",id_prog);
		printf(" %i  |",aux_tab_mem->memFisica);
		printf(" %i  |",aux_tab_mem->longitud);
		printf(" %i  |\n",aux_tab_mem->memLogica);
	}
	printf("--------------------------------------------------------\n");
	//todo destruir lista, si lo destruyo pasa algo malo?
}

void imp_mem_prin(){
	//todo hacer q imprima la memoria principal
	//ir buscando en la tabla de segmentos e imprimir segun de que proceso es tipo
	// "50###########################50##################BBBBBBBBBBBBBBBBBB53#####################
	//los B son de basura
}

void imp_cont_mem_prin(){
	int32_t offset, tam, i;

	printf ("--------------------------------\n"
			"Ingrese un offset\n");
	scanf("%i",&offset);
	printf ("Ingrese una cantidad de bytes\n");
	scanf("%i",&tam);
	if (tam + offset > config_get_int_value(ptrConfig,"tamanio")){
		printf ("Segmentation Fault\n");
	}else{
		pthread_mutex_lock(&mutex_mem_prin);
		for(i=offset;i<(offset+tam);i++)
			printf("%c",mem_prin[i]);
		printf("\n");
		pthread_mutex_unlock(&mutex_mem_prin);
	}
}

void encabezado(long byte, char *modo){
	printf("---------------** UMV: Unidad de Memoria Virtual **---------------\n");
	printf("Puerto: %s\n", config_get_string_value(ptrConfig,"puerto"));
	printf("Operando en modo: %s\n", modo);
	printf("Espacio reservado: %ld bytes\n", byte);
	printf("------------------------------------------------------------------\n\n");
}
