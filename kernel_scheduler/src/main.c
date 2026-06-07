#include "kernel_scheduler.h"

char* pathInicial;

int main(int argc, char* argv[]) {
    
    if (argc != 3)
    {
        printf("Uso: ./bin/kernel_scheduler [Archivo Config] [Path Proceso Inicial]\n");
        return EXIT_FAILURE;
    }
    //Se inicializa la estructura y configuracion
    t_kernel_scheduler* kernel_scheduler = iniciar_kernel_scheduler(argv[1]);
    verificar_kernel_scheduler(kernel_scheduler);
    // Conexión con Kernel Memory 
    kernel= kernel_scheduler; // Asignar el kernel_scheduler a la variable global para su uso en otros módulos
    conectar_con_kernel_memory(kernel_scheduler);

    iniciarPlanificadorLargoPlazo(kernel_scheduler);
    iniciarPlanificadorLCortoPlazo(kernel_scheduler);
    iniciarCPU();

    pathInicial = argv[2];

    // Crear el proceso inicial (PID 0) antes de esperar CPUs
    crearProceso(pathInicial, 0);
    kernel->procesoInicialCreado = true;

    //Crear el planificador de corto plazo
    pthread_t hilo_corto_plazo;
    if (pthread_create(&hilo_corto_plazo, NULL, loop_corto_plazo, NULL) != 0) {
        log_error(kernel_scheduler->logger, "No se pudo crear el hilo del Planificador de Corto Plazo");
        return EXIT_FAILURE;
    }
    pthread_detach(hilo_corto_plazo); // Lo independizamos para no tener que hacerle un join
    
    // despues esperar CPUs
    esperar_conexiones(kernel_scheduler);
    destruir_kernel_scheduler(kernel_scheduler);
    return 0;
}
