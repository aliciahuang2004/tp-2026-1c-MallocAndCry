#include "kernel_scheduler.h"

void* atender_cpu(void* socket_cpu_ptr){
    int socket_cpu = *(int*)socket_cpu_ptr;
    free(socket_cpu_ptr); // Liberamos el puntero que reservamos en el handshake
    while(1) {
        t_list* paquete = recibir_paquete(socket_cpu);
        if (!paquete) {
            log_error(kernel->logger, "Error al recibir pedido de syscall");
            return NULL;
        }

        int cod_op = *(int*) list_get(paquete, 0);
        int pidSolicitaSyscall = *(int*) list_get(paquete, 1);
        t_cpu_conectada* cpu_emisora = buscar_cpu_por_socket(socket_cpu);

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
                tomarMutex(pidSolicitaSyscall, nombreMutex, socket_cpu);
                break;
            }
            case MUTEX_UNLOCK:{
                char* nombreMutex = (char*) list_get(paquete, 2);
                log_info(kernel->logger, "## (<%d>) - Solicitó syscall: <MUTEX_UNLOCK>", pidSolicitaSyscall);
                liberarMutex(pidSolicitaSyscall, nombreMutex);
                enviarPIDAcpu(pidSolicitaSyscall,cpu_emisora);
                break;
            }
            case MEM_ALLOC: {//POSIBLE BLOQUEADO => PEDIMOS DESALOJO
                int idSegmento = *(int*) list_get(paquete, 2);
                int tamanio = *(int*) list_get(paquete, 3);
                log_info(kernel->logger, "## (<%d>) - Solicitó syscall: <MEM_ALLOC> idSegmento=%d tamanio=%d", pidSolicitaSyscall, idSegmento, tamanio);
                // Pasa a BLOCK sin liberar la CPU: la CPU queda "reservada" para este PID
                pasarProcesoExecABlockSinLiberar(pidSolicitaSyscall);
                asignarMemoria(pidSolicitaSyscall, idSegmento, tamanio);
                break;
            }
            case MEM_FREE: {// NO BLOQUEA
                int idSegmento = *(int*) list_get(paquete, 2);
                //KS DEBERIA ENVIAR ESTE DATO CON PROTOCOLO ELIMINACION_DE_SEGMENTO A TRAVÉS DEL SOCKET DE KM GUARDADO EN kernel_scheduler->socket_kernel_memory EN FUNCION CONECTAR_KERNEL_MEMORY 
                log_info(kernel->logger, "## (<%d>) - Solicitó syscall: <MEM_FREE> idSegmento=%d", pidSolicitaSyscall, idSegmento);
                liberarMemoria(pidSolicitaSyscall, idSegmento);
                enviarPIDAcpu(pidSolicitaSyscall, cpu_emisora);
                break;
            }
            case SLEEP:  {
                int tiempo_ms = *(int*) list_get(paquete, 2);
                log_info(kernel->logger, "## (<%d>) - Solicitó syscall: <SLEEP>", pidSolicitaSyscall);
                manejar_sleep(pidSolicitaSyscall, tiempo_ms, cpu_emisora);
                break;
            }
            case STDIN:{
                uint32_t dir_logica = *(uint32_t*) list_get(paquete, 2);
                uint32_t tamano = *(uint32_t*) list_get(paquete, 3);
                log_info(kernel->logger, "## PID: %d - Solicitó Syscall STDIN (dir=%u, tam=%u)", pidSolicitaSyscall, dir_logica, tamano);
                manejar_stdin(pidSolicitaSyscall, dir_logica, tamano, cpu_emisora);
                break;
            }
            case STDOUT:{
                uint32_t dir_logica = *(uint32_t*) list_get(paquete, 2);
                uint32_t tamano = *(uint32_t*) list_get(paquete, 3);
                log_info(kernel->logger, "## (<%d>) - Solicitó syscall: <STDOUT>", pidSolicitaSyscall);
                manejar_stdout(pidSolicitaSyscall, dir_logica, tamano, cpu_emisora);
                break;
            }
            case INIT_PROC: {// NO BLOQUEA
                char* path_script = (char*) list_get(paquete, 2);
                int prioridad = *(int*) list_get(paquete, 3);
                log_info(kernel->logger, "## CPU solicita creación de proceso: %s (prioridad %d)", path_script, prioridad);
                crearProceso(path_script, prioridad);
                enviarPIDAcpu(pidSolicitaSyscall,cpu_emisora);
                break;
            }
            case EXIT_PROC: {// NO BLOQUEA PERO DESALOJA PORQUE FINALIZA EL PROCESO
                log_info(kernel->logger, "## (<%d>) - Solicitó syscall: <EXIT_PROC>", pidSolicitaSyscall);
                finalizarProceso(pidSolicitaSyscall);
                liberar_cpu_y_notificar(cpu_emisora);
                break;
            }
            case PROCESO_DESALOJADO_PRIORIDAD: {
                log_info(kernel->logger, "## (<%d>) - Desalojado por prioridad", pidSolicitaSyscall);
                pasarProcesoExecAReady(pidSolicitaSyscall, cpu_emisora);
                break;
            }

            case SEG_FAULT: { 
                log_error(kernel->logger, "## (<%d>) - Finaliza ejecucion por Segmentation Fault (SEG_FAULT)", pidSolicitaSyscall);
                finalizarProceso(pidSolicitaSyscall);
                liberar_cpu_y_notificar(cpu_emisora);
                break;
            }

            default:
                log_warning(kernel->logger, "Operación desconocida de CPU: %d", cod_op);
                break;
        }

        list_destroy_and_destroy_elements(paquete, free);
    }
}