#include "programa.h"

int soc_kernel;

int main(int argc, char *argv[]){
	t_config *config=config_create("./programa/programa_config");//todo creo q esta ruta tendria q ser guardada en la variable de entorno ANSISOP_CONFIG
	printf("\n\n------------------------------Archivo Config----------------------------------------\n");
	char *ip_kernel=config_get_string_value(config, "IP_Kernel");
	char *puerto_kernel = config_get_string_value(config, "Puerto_Kernel");
	printf("	IP Kernel     = %s\n", ip_kernel);
	printf("	Puerto Kernel = %s\n", puerto_kernel);
	printf("------------------------------------------------------------------------------------\n\n");

	FILE *scriptAProcesar = fopen(argv[1],"r");
	int tamanioScript=tamanioArchivo(scriptAProcesar);
	char *script = malloc(tamanioScript);

	soc_kernel=socket_crear_client(puerto_kernel, ip_kernel);
	handshake_kernel(soc_kernel);

	int i=0;
	//Lee el script
	while(!feof(scriptAProcesar))
		script[i++]=fgetc(scriptAProcesar);
	fclose(scriptAProcesar);

	t_men_comun *mensaje_cod_prog = crear_men_comun(CODIGO_SCRIPT,script,tamanioScript);
	socket_send_comun(soc_kernel,mensaje_cod_prog);
	destruir_men_comun(mensaje_cod_prog);
	free(script);

	//espero la respuesta del kernel
	int fin_ejecucion = 0;
	t_men_comun *mensaje_recibido;
	while (fin_ejecucion != 1){
		mensaje_recibido = socket_recv_comun(soc_kernel);

		switch(mensaje_recibido->tipo){
		case IMPRIMIR_VALOR:
			printf("%d",atoi(mensaje_recibido->dato));
			break;
		case IMPRIMIR_TEXTO:
			printf("%s",mensaje_recibido->dato);
			break;
		case CPU_DESCONEC:
			printf("CPU DESCONECTADA\n");
			fin_ejecucion = 1;
			break;
		case CONEC_CERRADA:
			fin_ejecucion = 1;
			break;
		case FIN_EJECUCION:
			printf("Ejecucion finalizada");
			fin_ejecucion = 1;
			break;
		case MEM_OVERLOAD:
			fin_ejecucion = 1;
			printf("Memory Overload\n");
			break;
		default:
			sleep(1);
			printf("%i\n",mensaje_recibido->tipo);
			printf("El tipo de dato recibido es erroneo\n");
			break;
		}
		destruir_men_comun(mensaje_recibido);
	}

	close(soc_kernel);
	config_destroy(config);
	return 0;
}

int tamanioArchivo(FILE *archivo){
	fseek(archivo, 0L, SEEK_END);
	int tamanio = ftell(archivo);
	fseek(archivo,0L, SEEK_SET);
	return tamanio;
}

void handshake_kernel(){
	t_men_comun *men_hs = crear_men_comun(HS_PROG,"",1);
	socket_send_comun(soc_kernel,men_hs);
	destruir_men_comun(men_hs);

	men_hs = socket_recv_comun(soc_kernel);

	if(men_hs->tipo != HS_KERNEL)
		printf("ERROR se esperaba HS_KERNEL y se recibio %i\n",men_hs->tipo);
	destruir_men_comun(men_hs);
}
