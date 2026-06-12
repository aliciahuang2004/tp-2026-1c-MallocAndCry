#include "cpu.h"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Uso: ./bin/cpu [Archivo Config] [Identificador]\n");
        return EXIT_FAILURE;
    }

    t_cpu* cpu = iniciar_cpu(argv[1], argv[2]);

    // Conectamos primero al Kernel Scheduler y enviamos el handshake inmediato.
    if (conectar_kernel_scheduler(cpu) == -1) {
        log_error(cpu->logger, "Error en la conexion con Kernel Scheduler. Terminando programa.");
        liberar_cpu(cpu);
        return -1;
    }

    if (conectar_kernel_memory(cpu) == -1) {
        log_error(cpu->logger, "Error en la conexion con Kernel Memory. Terminando programa."); 
        liberar_cpu(cpu);
        return -1;
    }

    sem_init(&sem_contexto_recibido, 0, 0);
    sem_init(&sem_instruccion_recibida, 0, 0);

    pthread_t hilo_km;
    if (pthread_create(&hilo_km, NULL, escuchar_kernel_memory, cpu) != 0) {
        log_error(cpu->logger, "Error al crear el hilo de Kernel Memory");
        return -1;
    }
    pthread_detach(hilo_km);


    if (conectar_memory_stick(cpu, cpu->ip_memory_stick_inicial, cpu->puerto_memory_stick_inicial,0) == -1) {
        log_warning(cpu->logger, "Error en la conexion con memory stick.");
    }

    log_info(cpu->logger, "## CPU %s inicializada y conectada exitosamente", cpu->id);

    esperar_proceso(cpu);

    liberar_cpu(cpu);
    
    return 0;
}