#include "instrucciones.h"



void inicializar_proceso_memoria(int pid, char* path_relativo, t_kernel_memory* km) {
    char* path_absoluto = string_new();
    string_append(&path_absoluto, km->scripts_basePath);
    
    if (!string_ends_with(km->scripts_basePath, "/") && !string_starts_with(path_relativo, "/")) {
        string_append(&path_absoluto, "/");
    }
    string_append(&path_absoluto, path_relativo);

    char pid_str[10];
    sprintf(pid_str, "%d", pid);

    dictionary_put(km->paths_por_pid, pid_str, path_absoluto);
    log_info(km->logger, "Asignado path absoluto [%s] al PID: %d", path_absoluto, pid);
}

char* obtener_instruccion(int pid, int pc, t_kernel_memory* km) {
    char pid_str[10];
    sprintf(pid_str, "%d", pid);

    
    char* path_absoluto = dictionary_get(km->paths_por_pid, pid_str);
    if (path_absoluto == NULL) {
        log_error(km->logger, "No se encontro un archivo asociado al PID %d", pid);
        return NULL;
    }

   
    usleep(km->instruction_delay * 1000); 

   
    FILE* archivo = fopen(path_absoluto, "r");
    if (archivo == NULL) {
        log_error(km->logger, "Error al abrir el archivo de pseudocodigo: %s", path_absoluto);
        return NULL;
    }

    char* linea = NULL;
    size_t len = 0;
    ssize_t read;
    int linea_actual = 0;
    char* instruccion_encontrada = NULL;

  
    while ((read = getline(&linea, &len, archivo)) != -1) {
        if (linea_actual == pc) {
           
            if (linea[read - 1] == '\n') {
                linea[read - 1] = '\0';
            }
            
            instruccion_encontrada = string_duplicate(linea);
            break;
        }
        linea_actual++;
    }

    free(linea);
    fclose(archivo);

    
    if (instruccion_encontrada != NULL) {
        log_info(km->logger, "## Obtener instrucción - PID: %d - Instrucción: %s", pid, instruccion_encontrada);
    } else {
        log_warning(km->logger, "No se encontro la instruccion para el PC %d ", pc);
    }

    return instruccion_encontrada;
}