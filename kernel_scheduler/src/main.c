#include "kernel_scheduler.h"

// char* pathInicial;
t_kernel_scheduler* kernel = NULL;
int main(int argc, char* argv[]) {
    
    if (argc != 3){
        printf("Uso: ./bin/kernel_scheduler [Archivo Config] [Path Proceso Inicial]\n");
        return EXIT_FAILURE;
    }

    //Se inicializa la estructura y configuracion
    t_kernel_scheduler* kernel_scheduler = iniciar_kernel_scheduler(argv[1]);
    //*** verificar_kernel_scheduler(kernel_scheduler);

    // Conexión con Kernel Memory 
    kernel = kernel_scheduler; // Asignar el kernel_scheduler a la variable global para su uso en otros módulos
    conectar_con_kernel_memory();
    
    //*** iniciar_semaforos_datos_recibidos();
    pthread_t hilo_escucha_km;
    if (pthread_create(&hilo_escucha_km, NULL, atender_kernel_memory, NULL) != 0) {
        log_error(kernel_scheduler->logger, "No se pudo crear el hilo de escucha de Kernel Memory");
        return EXIT_FAILURE;
    }
    pthread_detach(hilo_escucha_km);

    inicializarColas();
    inicializarSemaforos();

    /*iniciarPlanificadorLargoPlazo(kernel_scheduler);
    iniciarPlanificadorLCortoPlazo(kernel_scheduler);
    iniciarCPU();*/

    //***pathInicial = argv[2];

    // Crear el proceso inicial (PID 0) antes de esperar CPUs

    crearProceso(argv[2], 0);

    pthread_t hiloMonitorPrioridades;
    pthread_create(&hiloMonitorPrioridades, NULL, monitorPrioridades, NULL);
    pthread_detach(hiloMonitorPrioridades);

    //Crear el planificador de corto plazo
    pthread_t hilo_corto_plazo;
    if (pthread_create(&hilo_corto_plazo, NULL, loop_corto_plazo, NULL) != 0) {
        log_error(kernel_scheduler->logger, "No se pudo crear el hilo del Planificador de Corto Plazo");
        return EXIT_FAILURE;
    }
    pthread_detach(hilo_corto_plazo); // Lo independizamos para no tener que hacerle un join
    //*** kernel->procesoInicialCreado = true;

    //Crear el planificador de corto plazo
    /*pthread_t hilo_corto_plazo;
    if (pthread_create(&hilo_corto_plazo, NULL, loop_corto_plazo, NULL) != 0) {
        log_error(kernel_scheduler->logger, "No se pudo crear el hilo del Planificador de Corto Plazo");
        return EXIT_FAILURE;
    }
    pthread_detach(hilo_corto_plazo); // Lo independizamos para no tener que hacerle un join
    */

    // despues esperar CPUs
    esperar_conexiones();
    // destruir_kernel_scheduler(kernel_scheduler);
    return 0;
}
