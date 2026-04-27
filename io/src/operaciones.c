#include "io.h"
#include <readline/readline.h>

void ejecutar_stdin(int pid, uint32_t datos_size, int socket_ks, t_log* logger) {
    log_info(logger, "## PID: %d - Ingrese %u caracteres:", pid, datos_size);
    
    // Pedir input al usuario
    char* input = readline("");
    if (input == NULL) {
        input = calloc(1, 1);
    }
            
    size_t input_len = strlen(input);
            
    // Crear buffer de respuesta exactamente del tamaño solicitado
    char* buffer_respuesta = calloc(datos_size, sizeof(char));
            
    if (datos_size > 0) {
        if (input_len >= datos_size) {
        // Si la entrada es mayor o igual, copiar solo los primeros N bytes
            memcpy(buffer_respuesta, input, datos_size);
            log_info(logger, "Entrada truncada a %u bytes (entrada original: %zu bytes)", datos_size, input_len);
        } else {
        // Si la entrada es menor, copiar y rellenar con '\0'
            memcpy(buffer_respuesta, input, input_len);
            // El resto ya está en '\0' por calloc
            log_info(logger, "Entrada completada con null terminators (%zu + %u bytes de padding)", input_len, datos_size - input_len);
                }
            }

    enviar_confirmacion_ks(socket_ks, pid, buffer_respuesta, datos_size, logger);
    free(buffer_respuesta);
}

void ejecutar_stdout(int pid, uint32_t datos_size, void* datos, int socket_ks, t_log* logger) {
    // Imprimir por pantalla y en el archivo de Log 
    log_info(logger, "Ejecutando STDOUT: imprimiendo %u bytes para PID %d", datos_size, pid);
    if (datos != NULL && datos_size > 0) {
        // Imprimir en pantalla
        printf("[IO STDOUT PID %d] ", pid);
        fwrite(datos, 1, datos_size, stdout);
        printf("\n");
        fflush(stdout);

        // Registrar en log
        char* datos_str = malloc(datos_size + 1);
        memcpy(datos_str, datos, datos_size);
        datos_str[datos_size] = '\0';
        free(datos_str);
    } else {
        log_warning(logger, "STDOUT: No hay datos para imprimir");
        }
    enviar_confirmacion_ks(socket_ks, pid, NULL, 0, logger);
}

void ejecutar_sleep(int pid, uint32_t tiempo_ms, int socket_ks, t_log* logger) {
   
    log_info(logger, "## PID: %d - Haciendo sleep por %u milisegundos.", pid, tiempo_ms);
    usleep(tiempo_ms * 1000); // usleep espera microsegundos
    enviar_confirmacion_ks(socket_ks, pid, NULL, 0, logger);

    
}