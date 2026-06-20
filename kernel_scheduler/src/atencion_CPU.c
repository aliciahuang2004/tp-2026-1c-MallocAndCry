#include "kernel_scheduler.h"

void* atender_cpu(void* arg){
    int socket_cpu = *(int*)arg;
    free(arg);

    log_info(kernel->logger, "CPU Listener: hilo iniciado en socket %d", socket_cpu);
    sem_post(&sem_hayCPUdisponible);
    
    while(1) {
        t_list* paquete = recibir_paquete(socket_cpu);
        if (!paquete) {
            log_error(kernel->logger, "Error al recibir pedido de syscall");
            t_cpu_conectada* cpu = buscar_cpu_por_socket(socket_cpu);
            if(cpu != NULL){
                finalizarProceso(cpu->pidEjecutando,DESCONEXION_CPU);
            return NULL;
        }

        int cod_op = *(int*) list_get(paquete, 0);
        int pidSolicitaSyscall = *(int*) list_get(paquete, 1);
        t_cpu_conectada* cpu_emisora = buscar_cpu_por_socket(socket_cpu);

        log_info(kernel->logger, "ID CPU: %d", cpu_emisora->id_cpu);

        switch (cod_op) {
            case MUTEX_CREATE:{
                char* nombreMutex = (char*) list_get(paquete, 2);
                log_info(kernel->logger, "## (<%d>) - Solicitó syscall: <MUTEX_CREATE>", pidSolicitaSyscall);
                crearMutex(nombreMutex);
                enviarPIDAcpu(pidSolicitaSyscall,cpu_emisora);
                break;
            }
            case MUTEX_LOCK:{
                char* nombreMutex = (char*) list_get(paquete, 2);
                log_info(kernel->logger, "## (<%d>) - Solicitó syscall: <MUTEX_LOCK>", pidSolicitaSyscall);
                tomarMutex(pidSolicitaSyscall, nombreMutex, cpu_emisora);
                break;
            }
            case MUTEX_UNLOCK:{
                char* nombreMutex = (char*) list_get(paquete, 2);
                log_info(kernel->logger, "## (<%d>) - Solicitó syscall: <MUTEX_UNLOCK>", pidSolicitaSyscall);
                liberarMutex(pidSolicitaSyscall, nombreMutex);
                // enviarPIDAcpu(pidSolicitaSyscall,cpu_emisora);
                break;
            }
            case MEM_ALLOC: {//POSIBLE BLOQUEADO => PEDIMOS DESALOJO
                /*//lineas agregadas para prueba en km 407 a 409,412,413,415 a 421.
                printf("\n\n*********** ENTRE A MEM_ALLOC ***********\n");//este log lo agregué porque en mis pruebas cuando cpu envía syscall mem_alloc a ks,aveces entraba en el switch de "atender_cliente_scheduler" y a veces en este
                fflush(stdout);
                */
                int idSegmento = *(int*) list_get(paquete, 2);
                int tamanio = *(int*) list_get(paquete, 3);
                printf("PID=%d SEG=%d TAM=%d\n",pidSolicitaSyscall,idSegmento,tamanio);
                log_info(kernel->logger, "## (<%d>) - Solicitó syscall: <MEM_ALLOC> idSegmento=%d tamanio=%d", pidSolicitaSyscall, idSegmento, tamanio);
                asignarMemoria(pidSolicitaSyscall,idSegmento, tamanio);
                break;
            }
            case MEM_FREE: {// NO BLOQUEA
                int idSegmento = *(int*) list_get(paquete, 2);
                log_info(kernel->logger, "## (<%d>) - Solicitó syscall: <MEM_FREE> idSegmento=%d", pidSolicitaSyscall, idSegmento);
                liberarMemoria(pidSolicitaSyscall, idSegmento);
                // en el ok deberia enviarlo
                enviarPIDAcpu(pidSolicitaSyscall,cpu_emisora);
                //KS DEBERIA ENVIAR ESTE DATO CON PROTOCOLO ELIMINACION_DE_SEGMENTO A TRAVÉS DEL SOCKET DE KM GUARDADO EN kernel_scheduler->socket_kernel_memory EN FUNCION CONECTAR_KERNEL_MEMORY 
                break;
            }
            case SLEEP:{
                int tiempo_ms = *(int*) list_get(paquete, 2);
                log_info(kernel->logger, "## (<%d>) - Solicitó syscall: <SLEEP>", pidSolicitaSyscall);
                liberarCPU(socket_cpu);
                manejar_sleep(pidSolicitaSyscall, tiempo_ms, cpu_emisora);
                break;
            }
            case STDIN:{
                uint32_t dir_fisica = *(uint32_t*) list_get(paquete, 2);
                uint32_t tamano = *(uint32_t*) list_get(paquete, 3);
                log_info(kernel->logger, "## PID: %d - Solicitó Syscall <STDIN> ", pidSolicitaSyscall);
                liberarCPU(socket_cpu);
                manejar_stdin(pidSolicitaSyscall, dir_fisica, tamano, cpu_emisora);
                break;
            }
            case STDOUT:{
                uint32_t dir_fisica = *(uint32_t*) list_get(paquete, 2);
                uint32_t tamano = *(uint32_t*) list_get(paquete, 3);
                log_info(kernel->logger, "## (<%d>) - Solicitó syscall: <STDOUT>", pidSolicitaSyscall);
                liberarCPU(socket_cpu);
                //manejar_stdout(pidSolicitaSyscall, dir_fisica, tamano, cpu_emisora);
                break;
            }
            case INIT_PROC: {// NO BLOQUEA
                char* path_script = (char*) list_get(paquete, 2);
                int prioridad = *(int*) list_get(paquete, 3);
                log_info(kernel->logger, "## PID: %d - Solicitó Syscall: <INIT_PROC> %s (prioridad %d)",pidSolicitaSyscall, path_script, prioridad);
                crearProceso(path_script, prioridad);
                enviarPIDAcpu(pidSolicitaSyscall,cpu_emisora);
                break;
            }
            case EXIT_PROC: {// NO BLOQUEA PERO DESALOJA PORQUE FINALIZA EL PROCESO
                log_info(kernel->logger, "## (<%d>) - Solicitó syscall: <EXIT_PROC>", pidSolicitaSyscall);
                finalizarProceso(pidSolicitaSyscall,EXIT_PROC);
                liberarCPU(cpu_emisora);                
                break;
            }
            case PROCESO_DESALOJADO_QUANTUM:{
                log_info(kernel->logger, "## (<%d>) - Proceso desalojado por quantum", pidSolicitaSyscall);
                // reencolar
                break;
            }
            case PROCESO_DESALOJADO_PRIORIDAD:{
                log_info(kernel->logger, "## (<%d>) - Proceso desalojado por prioridad", pidSolicitaSyscall);
                // reencolar en cola correspondiente
                break;
            }
            case PROCESO_DESALOJADO_COMPACTACION:{
                log_info(kernel->logger, "## (<%d>) - Proceso desalojado por prioridad", pidSolicitaSyscall);
                // reencolar al inicio
                break;
            }
            default:
                log_warning(kernel->logger, "Operación desconocida de CPU: %d", cod_op);
                break;
        }

        list_destroy_and_destroy_elements(paquete,free);
    }
}