#include "instrucciones.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>


bool es_registro_8bits(char* nombre_registro) {
    return (strcmp(nombre_registro, "AX") == 0 || 
            strcmp(nombre_registro, "BX") == 0 || 
            strcmp(nombre_registro, "CX") == 0 || 
            strcmp(nombre_registro, "DX") == 0);
}

void* obtener_registro(t_registros* registros, char* nombre_registro) {
    if (strcmp(nombre_registro, "AX") == 0) return &(registros->AX);
    if (strcmp(nombre_registro, "BX") == 0) return &(registros->BX);
    if (strcmp(nombre_registro, "CX") == 0) return &(registros->CX);
    if (strcmp(nombre_registro, "DX") == 0) return &(registros->DX);
    
    if (strcmp(nombre_registro, "EAX") == 0) return &(registros->EAX);
    if (strcmp(nombre_registro, "EBX") == 0) return &(registros->EBX);
    if (strcmp(nombre_registro, "ECX") == 0) return &(registros->ECX);
    if (strcmp(nombre_registro, "EDX") == 0) return &(registros->EDX);
    
    if (strcmp(nombre_registro, "SI") == 0) return &(registros->SI);
    if (strcmp(nombre_registro, "DI") == 0) return &(registros->DI);
    if (strcmp(nombre_registro, "PC") == 0) return &(registros->PC);
    
    return NULL;
}

uint32_t leer_valor_registro(t_registros* registros, char* nombre_registro) {
    void* reg_ptr = obtener_registro(registros, nombre_registro);
    if (reg_ptr == NULL) return 0;
    
    if (es_registro_8bits(nombre_registro)) {
        return (uint32_t)(*(uint8_t*)reg_ptr);
    } else {
        return *(uint32_t*)reg_ptr;
    }
}

// NOOP
void ejecutar_NOOP(t_cpu* cpu, t_contexto* ctx) {
    usleep(1000 * config_get_int_value(cpu->config, "INSTRUCTION_DELAY"));
    log_debug(cpu->logger, "Ejecutando NOOP: Simulando retardo de instrucción");
}

// SET
void ejecutar_SET(t_cpu* cpu,t_registros* reg, char* registro_destino, uint32_t valor) {
    void* reg_ptr = obtener_registro(reg, registro_destino);
    if (reg_ptr != NULL) {
        if (es_registro_8bits(registro_destino)) {
            *(uint8_t*)reg_ptr = (uint8_t)valor;
        } else {
            *(uint32_t*)reg_ptr = valor;
        }
    }
    log_debug(cpu->logger, "Ejecutando SET: Asignando valor %u al registro %s", valor, registro_destino);
}

// SUM
void ejecutar_SUM(t_cpu* cpu,t_registros* reg, char* registro_destino, char* registro_origen) {
    uint32_t valor_origen = leer_valor_registro(reg, registro_origen);
    void* reg_dest_ptr = obtener_registro(reg, registro_destino);
    
    if (reg_dest_ptr != NULL) {
        if (es_registro_8bits(registro_destino)) {
            *(uint8_t*)reg_dest_ptr += (uint8_t)valor_origen;
        } else {
            *(uint32_t*)reg_dest_ptr += valor_origen;
        }
    }
    log_debug(cpu->logger, "Ejecutando SUM: Sumando valor de registro %s al registro %s", registro_origen, registro_destino);
}

// SUB
void ejecutar_SUB(t_cpu* cpu,t_registros* reg, char* registro_destino, char* registro_origen) {
   uint32_t valor_origen = leer_valor_registro(reg, registro_origen);
    void* reg_dest_ptr = obtener_registro(reg, registro_destino);
    
    if (reg_dest_ptr != NULL) {
        if (es_registro_8bits(registro_destino)) {
            *(uint8_t*)reg_dest_ptr -= (uint8_t)valor_origen;
        } else {
            *(uint32_t*)reg_dest_ptr -= valor_origen;
        }
    }
    log_debug(cpu->logger, "Ejecutando SUB: Restando valor de registro %s al registro %s", registro_origen, registro_destino);
}

// JNZ
void ejecutar_JNZ(t_cpu* cpu,t_registros* reg, char* registro_evaluado, uint32_t nueva_instruccion) {
    uint32_t valor = leer_valor_registro(reg, registro_evaluado);
    // Solo actualiza el PC si el registro evaluado es distinto de 0
    if (valor != 0) {
        reg->PC = nueva_instruccion;
    }
    log_debug(cpu->logger, "Ejecutando JNZ: Evaluando registro %s para saltar a instrucción %u", registro_evaluado, nueva_instruccion);
}

// MOV_IN:
int ejecutar_MOV_IN(t_cpu* cpu, t_contexto* ctx, char* registro_datos) {
    
    log_debug(cpu->logger, "Ejecutando MOV_IN con destino en registro: %s", registro_datos);
    return 1; // 1 si fue exitoso, 0 si hubo SEG_FAULT para romper el ciclo
}

// MOV_OUT
int ejecutar_MOV_OUT(t_cpu* cpu, t_contexto* ctx, char* registro_datos) {
   
   log_debug(cpu->logger, "Ejecutando MOV_OUT con datos en registro: %s", registro_datos); 
    return 1;
}

// COPY_MEM
int ejecutar_COPY_MEM(t_cpu* cpu, t_contexto* ctx, char* registro_tamano) {

    log_debug(cpu->logger, "Ejecutando COPY_MEM con tamaño: %s", registro_tamano);
    return 1;
}

void ejecutar_SYSCALL(t_cpu* cpu, t_contexto* ctx, t_instruccion_decodificada instruccion){
    log_info(cpu->logger, "Delegando SYSCALL %s al Kernel Scheduler", instruccion.nombre_operacion);

    // se guarda el contexto en memoria
    enviar_contexto_a_memoria(cpu, ctx);

    t_paquete* paquete= NULL;

    if (instruccion.identificador_operacion == INST_SLEEP) {
        paquete = crear_paquete(SLEEP, crear_buffer());
        agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
        
        int tiempo = atoi(instruccion.argumento_operando_destino); // Convertimos el string a int
        agregar_a_paquete(paquete, &tiempo, sizeof(int));
    }
    else if (instruccion.identificador_operacion == INST_MUTEX_CREATE) {
        paquete = crear_paquete(MUTEX_CREATE, crear_buffer());
        agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
        agregar_a_paquete(paquete, instruccion.argumento_operando_destino, strlen(instruccion.argumento_operando_destino) + 1);
    }
    else if (instruccion.identificador_operacion == INST_MUTEX_LOCK) {
        paquete = crear_paquete(MUTEX_LOCK, crear_buffer());
        agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
        agregar_a_paquete(paquete, instruccion.argumento_operando_destino, strlen(instruccion.argumento_operando_destino) + 1);
    }
    else if (instruccion.identificador_operacion == INST_MUTEX_UNLOCK) {
        paquete = crear_paquete(MUTEX_UNLOCK, crear_buffer());
        agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
        agregar_a_paquete(paquete, instruccion.argumento_operando_destino, strlen(instruccion.argumento_operando_destino) + 1);
    }
    else if (instruccion.identificador_operacion == INST_INIT_PROC) {
        paquete = crear_paquete(INIT_PROC, crear_buffer());
        agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
        agregar_a_paquete(paquete, instruccion.argumento_operando_destino, strlen(instruccion.argumento_operando_destino) + 1); // Path
        
        int prioridad = atoi(instruccion.argumento_operando_origen); // Convertimos la prioridad a int
        agregar_a_paquete(paquete, &prioridad, sizeof(int));
    }
    else if (instruccion.identificador_operacion == INST_EXIT) {
        paquete = crear_paquete(EXIT_PROC, crear_buffer());
        agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
    }
    else if (instruccion.identificador_operacion == INST_MEM_ALLOC) {
        paquete = crear_paquete(MEM_ALLOC, crear_buffer());
        agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
       
        int id_segmento = atoi(instruccion.argumento_operando_destino);
        int tamanio = atoi(instruccion.argumento_operando_origen);
        
        agregar_a_paquete(paquete, &id_segmento, sizeof(int));
        agregar_a_paquete(paquete, &tamanio, sizeof(int));
    }
    else if (instruccion.identificador_operacion == INST_MEM_FREE) {
        paquete = crear_paquete(MEM_FREE, crear_buffer());
        agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
        
        // Un solo argumento numérico directo
        int id_segmento = atoi(instruccion.argumento_operando_destino);
        
        agregar_a_paquete(paquete, &id_segmento, sizeof(int));
    }
    else if (instruccion.identificador_operacion == INST_STDOUT) {
        paquete = crear_paquete(STDOUT, crear_buffer());
        agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
        
        // nos manda los nombres de los registros: "AX" "BX"
        // tenemos que leer qué valor numerico tienen adentro antes de mandar el paquete
        uint32_t dir_logica = leer_valor_registro(&(ctx->registros), instruccion.argumento_operando_destino);
        uint32_t tamanio = leer_valor_registro(&(ctx->registros), instruccion.argumento_operando_origen);
        
        agregar_a_paquete(paquete, &dir_logica, sizeof(uint32_t));
        agregar_a_paquete(paquete, &tamanio, sizeof(uint32_t));
    }
    else if (instruccion.identificador_operacion == INST_STDIN) {
        paquete = crear_paquete(STDIN, crear_buffer());
        agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
        
        uint32_t dir_logica = leer_valor_registro(&(ctx->registros), instruccion.argumento_operando_destino);
        uint32_t tamanio = leer_valor_registro(&(ctx->registros), instruccion.argumento_operando_origen);
        
        agregar_a_paquete(paquete, &dir_logica, sizeof(uint32_t));
        agregar_a_paquete(paquete, &tamanio, sizeof(uint32_t));
    }
    /*// avisamos al Scheduler que el proceso fue desalojado por una syscall
    // Usamos el codigo PROCESO_DESALOJADO y enviamos la Syscall y sus parametros para que el Kernel la procese
    t_paquete* paquete = crear_paquete(PROCESO_DESALOJADO, crear_buffer());
    agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
    agregar_a_paquete(paquete, instruccion.nombre_operacion, strlen(instruccion.nombre_operacion) + 1);
    

    // enviamos argumentos
    if (instruccion.argumento_operando_destino) {
        agregar_a_paquete(paquete, instruccion.argumento_operando_destino, strlen(instruccion.argumento_operando_destino) + 1);
    } else {
        agregar_a_paquete(paquete, "", 1);
    }
    
    if (instruccion.argumento_operando_origen) {
        agregar_a_paquete(paquete, instruccion.argumento_operando_origen, strlen(instruccion.argumento_operando_origen) + 1);
    } else {
        agregar_a_paquete(paquete, "", 1);
    }

*/
    if(paquete != NULL) {
        log_debug(cpu->logger, "Enviando paquete de SYSCALL %s al Kernel Scheduler", instruccion.nombre_operacion);
        enviar_paquete(paquete, cpu->socket_kernel_scheduler, cpu->logger);
        eliminar_paquete(paquete);
    }
    
}