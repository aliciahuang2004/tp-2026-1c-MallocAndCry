#ifndef SUELTOS_H_
#define SUELTOS_H_

#include <commons/log.h>     // Para t_log
#include <commons/config.h>  // Para t_config
#include <stdlib.h>          // Para exit
#include <pthread.h>    // para los hilos
#include <strings.h>

/**
 * @brief Crea e inicializa un logger con configuración específica
 * 
 * @details Esta función encapsula la creación de loggers de la biblioteca Commons,
 *          proporcionando manejo de errores robusto y validación de parámetros.
 * 
 * @param[in] logger        Nombre del archivo de log (ej: "worker.log")
 * @param[in] process_name  Identificador del proceso en los logs (ej: "[WORKER]")
 * @param[in] visible       Si es true, muestra logs en consola además del archivo
 * @param[in] log_level     Nivel de logging (LOG_LEVEL_TRACE, LOG_LEVEL_DEBUG, etc.)
 * 
 * @return Puntero al logger inicializado
 * @retval t_log* Logger creado exitosamente
 * @retval NULL   Si falla la creación (ya no debería ocurrir por el exit())
 * 
 * @note La función termina el programa si no puede crear el logger, ya que
 *       el logging es considerado crítico para el diagnóstico del sistema.
 * 
 * @warning Asegúrese de que el directorio de logs exista y tenga permisos
 *          de escritura, especialmente en entornos de producción.
 * 
 * @pre Los parámetros deben ser strings válidos (no NULL)
 * @post El logger está listo para usar con log_info(), log_error(), etc.
 * 
 * @code
 * // Ejemplo de uso:
 * t_log* logger = iniciar_logger("mi_modulo.log", "[MODULO]", true, LOG_LEVEL_INFO);
 * log_info(logger, "Logger inicializado correctamente");
 * @endcode
 */
t_log *iniciar_logger(char *logger, char *process_name, bool visible, t_log_level log_level);

t_config *iniciar_config(t_log *logger, char *path); 

t_log_level obtener_log_level(char* level);

void terminar_programa(t_log *logger, t_config *config);

#endif