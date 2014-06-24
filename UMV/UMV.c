#include "UMV.h"
int quit_sistema = 1;
char *mem_prin;
int retardo = 0;
char alg_actual;
t_dictionary *tablaProgramas;
t_dictionary *listaAuxiliar;
t_config *ptrConfig;
pthread_mutex_t mutex_mem_prin = PTHREAD_MUTEX_INITIALIZER;

int main(){
	srand(time(0));
	//crear configuracion y solicitar memoria
	ptrConfig = config_create("umv_config.txt");
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
	int listen_soc = socket_crear_server(config_get_string_value(ptrConfig,"puerto"));
	int new_soc;
	t_men_comun *men_hs;
	pthread_t hilo_conec_kernel;
	t_param_conec_kernel *param_kernel;
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
			//acordarse de responder el handshake
			continue;
		}
		printf("ERROR se esperaba recibir un tipo handshake y se recibio %i", men_hs->tipo);
	}
	socket_cerrar(listen_soc);
	return NULL;
}

void *admin_conec_kernel(t_param_conec_kernel *param){
	t_men_comun *men_hs= crear_men_comun(HS_UMV,NULL,0);
	socket_send_comun(param->soc,men_hs);
	t_men_ped_seg *men_ped;

	while(quit_sistema){
		men_ped = socket_recv_ped_seg(param->soc);
		switch (men_ped->tipo) {
		case PED_MEM_SEG_COD:
			//todo
			break;
		case PED_MEM_IND_ETI:
			//todo
			break;
		case PED_MEM_IND_COD:
			//todo
			break;
		case PED_MEM_SEG_STACK:
			//todo
			break;
		default:
			printf("ERROR tipo de dato recibio %i erroneo", men_ped->tipo);
			break;
		}
	}
	return NULL;
}

void *admin_conec_cpu(){


	return NULL;
}

bool compararListaAuxiliar(ListAuxiliar *nodo1, ListAuxiliar *nodo2){
	return ((nodo1->ptrInicio) < (nodo2->ptrInicio));
}

void compactar(){
	ListAuxiliar *p1 = malloc(sizeof(ListAuxiliar));
	ListAuxiliar *p2 = malloc(sizeof(ListAuxiliar));
	int x;
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
			int aux = (p1->ptrFin)+1;
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

void crearSegmento(char *id_Prog, int tamanio){
	void *lista;
	int memEstaOk;
	TabMen *nodoTab = malloc(sizeof(TabMen));
	/*pregunta si en la tabla de programas existe el id de prog,
		 si no existe se agrega el key al diccionario y si existe
		 se agrega un nodo a la lista de segmentos, controlando que
		 la memoria logica no se pise dentro del mismo programa */
	nodoTab->longitud = tamanio;
	nodoTab->memFisica = asignarMemoria(tamanio);

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
}

int controlarMemPisada(void *lista, int numMemoria, int tamanio){
	int x;
	TabMen *nodo;
	for (x=0;list_size(lista)-1;x++){
		nodo = list_get(lista,x);
		if ((((nodo->memLogica) <= numMemoria) && ((nodo->memLogica + nodo->longitud) >= numMemoria)) || (((nodo->memLogica) <= numMemoria+tamanio) && ((nodo->memLogica + nodo->longitud) >= numMemoria+tamanio)) || ((numMemoria <= nodo->memLogica) && (numMemoria >= (nodo->memLogica+nodo->longitud)))){
				return 0;
		}
	}
	return 1;
}

int asignarMemoria(int tamanio){
	ListAuxiliar *p1 = malloc(sizeof(ListAuxiliar));
	ListAuxiliar *p2 = malloc(sizeof(ListAuxiliar));
	int x;
	int espacioDisponible;
	int hayEspacio;
	int seCompacto;
	struct peorEspacio {
		int espacio;
		int offset;
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

int asignarMemoriaAleatoria(int tamanio){
	int numero;
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

void *solicitarBytes (int base, int offset, int tamanio){
	//se tiene el proceso activo
	/*revisar como se obtiene la clave id_proceso*/char *id_proceso;
	void *lista;
	TabMen *segmento;
	char *b;
	b = malloc(tamanio);
	lista = dictionary_get(tablaProgramas, id_proceso);
	// encontrar el nodo(segmento) en donde coincide la base y la memoria logica
	segmento = encontrarSegmento (lista,base);
	// teniendo el segmento, controlar que no se excedan los limites
	if (base+offset > (segmento->memLogica+segmento->longitud) || base+offset+tamanio > (segmento->memLogica+segmento->longitud)){
		//SEGMENTATION FAULT
	}
	else{
		void *origen;
		origen = mem_prin+segmento->memFisica+offset;
		memcpy (b,origen,tamanio);
		//hay q enviar b
	}
	return b;
}

//revisar el tipo que devuelve
//¿q devolver si no encuentra el segmento?
TabMen *encontrarSegmento(void *lista, int base){
	int x;
	TabMen *nodo;
	for (x=0;list_size(lista)-1;x++){
		nodo = list_get(lista,x);
		if (nodo->memLogica == base){
			return nodo;
		}
	}
}

void almacenarBytes (int base,int offset,int tamanio, char *buffer){
	//encontrar el segmento al q pertenece la base
	char *id_proceso;
	void *lista;
	TabMen *segmento;
	lista = dictionary_get(tablaProgramas, id_proceso);
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
	int base, offset, tam, id_proc;

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
	int t;

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
	char id_proc;

	printf ("--------------------------------\n"
			"Elija el id_proceso\n"
			"Ingrese 0 si desea imprimir todos\n");
	scanf("%c",&id_proc);
	int resp = imp_tablas_segmentos(id_proc);
	if (resp == -1)
		printf("El proceso no existe");
}

int imp_tablas_segmentos(int id_proc){
	//todo hacer funcion q imprimia las tablas de segmento
	//recibira un int y devuelve -1 si no existe el proceso
	return -1;
}

void imp_mem_prin(){
	//todo hacer q imprima la memoria principal
	//ir buscando en la tabla de segmentos e imprimir segun de que proceso es tipo
	// "50###########################50##################BBBBBBBBBBBBBBBBBB53#####################
	//los B son de basura
}

void imp_cont_mem_prin(){
	int offset, tam, i;

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
