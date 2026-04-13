#include "conexion.h"
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

	getaddrinfo(NULL, puerto, &hints, &servinfo);

	int socket_servidor = socket(servinfo->ai_family,
								 servinfo->ai_socktype,
								 servinfo->ai_protocol);

	// permite que varios sockets se puedan bindear a un puerto al mismo tiempo,
	// siempre y cuando pertenezcan al mismo usuario
	// setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int));

	bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);
	listen(socket_servidor, SOMAXCONN);

	freeaddrinfo(servinfo);
	// log_trace(logger, "Listo para escuchar a mi cliente");

	return socket_servidor;
}

int esperar_cliente(int socket_servidor)
{
	// Aceptamos un nuevo cliente
	int socket_cliente = accept(socket_servidor, NULL, NULL);

	return socket_cliente;
}
