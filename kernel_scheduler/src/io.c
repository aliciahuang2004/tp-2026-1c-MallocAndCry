#include "kernel_scheduler.h"

void* obtenerDatosDeKM(uint32_t dir_logica, uint32_t tamanio) {
    if (kernel->socket_kernel_memory == -1) {
        log_error(kernel->logger, "Error: Socket de Kernel Memory no disponible");
        return NULL;
    }

    pthread_mutex_lock(&mutex_lectura_km);

    t_buffer* buffer = crear_buffer();
    t_paquete* paquete = crear_paquete(LECTURA_DE_DATOS, buffer);
    agregar_a_paquete(paquete, &dir_logica, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tamanio, sizeof(uint32_t));

    int resultado = enviar_paquete(paquete, kernel->socket_kernel_memory, kernel->logger);
    eliminar_paquete(paquete);

    if (resultado != 0) {
        log_error(kernel->logger, "Error al solicitar datos a KM (dir=%u, tam=%u)", dir_logica, tamanio);
        pthread_mutex_unlock(&mutex_lectura_km);
        return NULL;
    }

    sem_wait(&sem_datos_listos);  // el listener hace sem_post cuando llega DATOS_LEIDOS

    void* datos = malloc(km_datos_size);
    memcpy(datos, km_datos_buffer, km_datos_size);
    free(km_datos_buffer);
    km_datos_buffer = NULL;

    log_debug(kernel->logger, "Datos obtenidos de KM: dir=%u, tamanio=%u bytes", dir_logica, km_datos_size);

    pthread_mutex_unlock(&mutex_lectura_km);
    return datos;
}

void manejar_sleep(int pid, int tiempo_ms, t_cpu_conectada* cpu) {
    t_pcb* pcb = pasarProcesoExecABlockSinLiberar(pid);
    if (pcb == NULL) {
        log_error(kernel->logger, "Error: No se encontró el proceso %d en colaEXEC", pid);
        liberar_cpu_y_notificar(cpu);
        return;
    }

    // Crear solicitud de IO
    uint32_t datos_size = sizeof(int);
    void* datos = malloc(datos_size);
    memcpy(datos, &tiempo_ms, datos_size);
    t_solicitud_io* solicitud = crear_solicitud_io(pid, pcb, OP_SLEEP, datos_size, datos);

    // Encolar en cola de IO SLEEP
    t_queue* cola_tipo = obtener_cola_bloqueados_por_tipo(IO_SLEEP);
    pthread_mutex_t* mutex_tipo = obtener_mutex_cola_por_tipo(IO_SLEEP);

    pthread_mutex_lock(mutex_tipo);
    queue_push(cola_tipo, solicitud);
    pthread_mutex_unlock(mutex_tipo);

    // Buscar interfaz libre y enviar
    t_interfaz_conectada* interfaz_libre = buscar_interfaz_libre_por_tipo(IO_SLEEP);
    if (interfaz_libre != NULL) {
        interfaz_libre->ocupada = true;
        log_info(kernel->logger, "Administración: Interfaz [%s] libre. Enviando solicitud de PID %d por red.",
                 interfaz_libre->nombre, pid);
        enviar_operacion_a_io(interfaz_libre, solicitud);
    } else {
        log_info(kernel->logger, "## Interfaz SLEEP ocupada. PID %d queda en cola de espera.", pid);
    }

    liberar_cpu_y_notificar(cpu);
}

void manejar_stdin(int pid, uint32_t dir_logica, uint32_t tamano, t_cpu_conectada* cpu) {
    t_pcb* pcb = pasarProcesoExecABlockSinLiberar(pid);
    if (pcb == NULL) {
        log_error(kernel->logger, "Error: No se encontró el proceso %d en colaEXEC", pid);
        liberar_cpu_y_notificar(cpu);
        return;
    }

    // Crear solicitud de IO
    uint32_t datos_size = sizeof(uint32_t) * 2;
    uint32_t* params = malloc(datos_size);
    params[0] = dir_logica;
    params[1] = tamano;
    t_solicitud_io* solicitud = crear_solicitud_io(pid, pcb, OP_STDIN, datos_size, params);

    // Encolar en cola de IO STDIN
    t_queue* cola_tipo = obtener_cola_bloqueados_por_tipo(IO_STDIN);
    pthread_mutex_t* mutex_tipo = obtener_mutex_cola_por_tipo(IO_STDIN);

    pthread_mutex_lock(mutex_tipo);
    queue_push(cola_tipo, solicitud);
    pthread_mutex_unlock(mutex_tipo);

    // Buscar interfaz libre y enviar
    t_interfaz_conectada* interfaz_libre = buscar_interfaz_libre_por_tipo(IO_STDIN);
    if (interfaz_libre != NULL) {
        interfaz_libre->ocupada = true;
        log_info(kernel->logger, "Administración: Interfaz [%s] libre. Enviando solicitud de PID %d por red.",
                 interfaz_libre->nombre, pid);
        enviar_operacion_a_io(interfaz_libre, solicitud);
    } else {
        log_info(kernel->logger, "## Interfaz STDIN ocupada. PID %d queda en cola de espera.", pid);
    }

    liberar_cpu_y_notificar(cpu);
}

void manejar_stdout(int pid, uint32_t dir_logica, uint32_t tamano, t_cpu_conectada* cpu) {
    t_pcb* pcb = pasarProcesoExecABlockSinLiberar(pid);
    if (pcb == NULL) {
        log_error(kernel->logger, "Error: No se encontró el proceso %d en colaEXEC", pid);
        liberar_cpu_y_notificar(cpu);
        return;
    }

    // Pedir datos a Kernel Memory
    void* datos = obtenerDatosDeKM(dir_logica, tamano);
    if (datos == NULL) {
        log_error(kernel->logger, "Error: No se pudieron obtener datos de KM para STDOUT. PID: %d", pid);
    }

    // Crear solicitud de IO con los datos obtenidos de KM
    t_solicitud_io* solicitud = crear_solicitud_io(pid, pcb, OP_STDOUT, tamano, datos);

    // Encolar en cola de IO STDOUT
    t_queue* cola_tipo = obtener_cola_bloqueados_por_tipo(IO_STDOUT);
    pthread_mutex_t* mutex_tipo = obtener_mutex_cola_por_tipo(IO_STDOUT);

    pthread_mutex_lock(mutex_tipo);
    queue_push(cola_tipo, solicitud);
    pthread_mutex_unlock(mutex_tipo);

    // Buscar interfaz libre y enviar
    t_interfaz_conectada* interfaz_libre = buscar_interfaz_libre_por_tipo(IO_STDOUT);
    if (interfaz_libre != NULL) {
        interfaz_libre->ocupada = true;
        log_info(kernel->logger, "Administración: Interfaz [%s] libre. Enviando solicitud de PID %d por red.",
                 interfaz_libre->nombre, pid);
        enviar_operacion_a_io(interfaz_libre, solicitud);
    } else {
        log_info(kernel->logger, "## Interfaz STDOUT ocupada. PID %d queda en cola de espera.", pid);
    }

    liberar_cpu_y_notificar(cpu);
}