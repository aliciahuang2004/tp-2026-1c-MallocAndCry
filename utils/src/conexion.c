#include "../include/conexion.h"
#include <commons/log.h>	// Para t_log
#include <commons/config.h> // Para t_config
#include <netdb.h>			// Para getaddrinfo, struct addrinfo
#include <string.h>			// Para memset
#include <stdlib.h>			// Para exit
#include <unistd.h>			// Para close
#include <errno.h>

// Conexion del lado del cliente
int crear_conexion(t_log *logger, char *ip, char *port)
{
    int client_socket;
    struct addrinfo hints;
    struct addrinfo *server_info;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int result = getaddrinfo(ip, port, &hints, &server_info);
    if (result != 0)
    {
        log_error(logger, "getaddrinfo falló: %s", gai_strerror(result));
        return -1;
    }

    client_socket = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (client_socket == -1)
    {
        log_error(logger, "Error al crear socket: %s", strerror(errno));
        freeaddrinfo(server_info);
        return -1;
    }

    // Connect socket con timeout opcional
    int conection = connect(client_socket, server_info->ai_addr, server_info->ai_addrlen);
    if (conection == -1)
    {
        log_error(logger, "Error al conectar: %s", strerror(errno));
        close(client_socket);
        freeaddrinfo(server_info);
        return -1;  // Cambio exit(1) por return -1
    }

    freeaddrinfo(server_info);
    return client_socket;
}

// Conexion del lado del server
int iniciar_servidor(const char* puerto)
{
	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	int result = getaddrinfo(NULL, puerto, &hints, &servinfo);
	if (result != 0) {
		fprintf(stderr, "Error en getaddrinfo para puerto %s: %s\n", puerto, gai_strerror(result));
		return -1;
	}

	int socket_servidor = socket(servinfo->ai_family,
						 servinfo->ai_socktype,
						 servinfo->ai_protocol);
	if (socket_servidor == -1) {
		fprintf(stderr, "Error al crear socket servidor: %s\n", strerror(errno));
		freeaddrinfo(servinfo);
		return -1;
	}

	if (bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
		fprintf(stderr, "Error al bindear socket servidor: %s\n", strerror(errno));
		close(socket_servidor);
		freeaddrinfo(servinfo);
		return -1;
	}

	if (listen(socket_servidor, SOMAXCONN) == -1) {
		fprintf(stderr, "Error en listen del socket servidor: %s\n", strerror(errno));
		close(socket_servidor);
		freeaddrinfo(servinfo);
		return -1;
	}

	freeaddrinfo(servinfo);
	return socket_servidor;
}

int esperar_cliente(int socket_servidor)
{
	int socket_cliente = accept(socket_servidor, NULL, NULL);
	return socket_cliente;
}

void liberar_conexion(int socket_cliente)
{
	close(socket_cliente);
}
