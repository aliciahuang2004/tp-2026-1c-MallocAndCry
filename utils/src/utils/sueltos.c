#include "sueltos.h"
#include <commons/log.h>
#include <stdio.h>          // Para printf

t_log *iniciar_logger(char *logger, char *process_name, bool visible, t_log_level log_level)
{
    t_log *new_logger = log_create(logger, process_name, visible, log_level);
    if (new_logger == NULL)
    {
        fprintf(stderr, "ERROR CRÍTICO: No se pudo crear logger '%s' para proceso '%s'\n", 
            logger, process_name);
        exit(EXIT_FAILURE);
    }
    return new_logger;
}

t_config *iniciar_config(t_log *logger, char *path)
{
    t_config *new_config = config_create(path);
    if (new_config == NULL)
    {
        log_error(logger, "Error al crear el puntero al archivo de configuracion: %s", path);
        exit(EXIT_FAILURE);
    }
    log_debug(logger, "Archivo de configuración cargado correctamente: %s", path);

    return new_config;
}

t_log_level obtener_log_level(char* level)
{
    if (strcasecmp(level, "TRACE")   == 0) return LOG_LEVEL_TRACE;
    if (strcasecmp(level, "DEBUG")   == 0) return LOG_LEVEL_DEBUG;
    if (strcasecmp(level, "INFO")    == 0) return LOG_LEVEL_INFO;
    if (strcasecmp(level, "WARNING") == 0) return LOG_LEVEL_WARNING;
    if (strcasecmp(level, "ERROR")   == 0) return LOG_LEVEL_ERROR;

    // Si viene algo mal, default:
    return LOG_LEVEL_INFO;
}

void terminar_programa(t_log *logger, t_config *config)
{
    log_destroy(logger);
    config_destroy(config);
}
