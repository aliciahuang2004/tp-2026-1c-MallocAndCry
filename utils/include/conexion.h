#ifndef CONEXION_H_
#define CONEXION_H_

#include <sys/socket.h>
#include <commons/log.h>


// Funciones de servidor

/**
 * @brief Inicia un servidor socket en escucha
 * @details Crea un socket servidor, lo bindea a una dirección IP y puerto,
 *          y lo pone en modo escucha para aceptar conexiones entrantes
 * @param[in] ip Dirección IP a bindear (NULL para todas las interfaces)
 * @param[in] puerto Puerto en el que escuchará el servidor
 * @return Socket servidor en modo escucha, o -1 si hay error
 * @retval int Socket válido si la inicialización es exitosa
 * @retval -1 Si ocurre un error en la creación o configuración del socket
 * @note El socket retornado debe ser cerrado con close() cuando ya no se necesite
 * @warning La función no maneja errores de bind() ni listen() en esta implementación
 * @pre El puerto debe estar disponible y la IP válida
 * @post El socket está configurado y escuchando conexiones entrantes
 * @ingroup Conexion
 * */
int iniciar_servidor(const char* puerto);

/**
 * @brief Acepta una nueva conexión cliente en un socket servidor
 * @details Bloquea hasta que un cliente se conecte al socket servidor
 * @param[in] socket_servidor Socket servidor en modo escucha
 * @return Socket del cliente conectado, o -1 si hay error
 * @retval int Socket válido del cliente recién conectado
 * @retval -1 Si ocurre un error al aceptar la conexión
 * @note El socket retornado debe ser cerrado con close() cuando ya no se necesite
 * @warning La función bloquea la ejecución hasta que llegue una conexión
 * @pre El socket_servidor debe estar en modo escucha (después de listen())
 * @post Se ha establecido una nueva conexión cliente-servidor
 * @ingroup Conexion
 * */
int esperar_cliente(int socket_servidor);

// Función genérica para liberar una conexión
void liberar_conexion(int socket_cliente);

// no es preferible no enlazar conexion y logger dentro de una misma unidad logica?
// podemos hacerlo accesible como variable global
//int crear_conexion(const char* ip, int puerto);
int crear_conexion(t_log *logger, char *ip, char *port);

#endif