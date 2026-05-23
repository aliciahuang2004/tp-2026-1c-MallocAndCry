#include "instrucciones.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

// NOOP
void ejecutar_NOOP(t_cpu* cpu, t_contexto* ctx) {
    usleep(1000 * config_get_int_value(cpu->config, "INSTRUCTION_DELAY"));
    log_debug(cpu->logger, "Ejecutando NOOP: Simulando retardo de instrucción");
}

// SET
void ejecutar_SET(t_registros* reg, char* registro_destino, uint32_t valor) {
    log_debug(cpu->logger, "Ejecutando SET: Asignando valor %u al registro %s", valor, registro_destino);
}

// SUM
void ejecutar_SUM(t_registros* reg, char* registro_destino, char* registro_origen) {
    log_debug(cpu->logger, "Ejecutando SUM: Sumando valor de registro %s al registro %s", registro_origen, registro_destino);
}

// SUB
void ejecutar_SUB(t_registros* reg, char* registro_destino, char* registro_origen) {
   log_debug(cpu->logger, "Ejecutando SUB: Restando valor de registro %s al registro %s", registro_origen, registro_destino);
}

// JNZ
void ejecutar_JNZ(t_registros* reg, char* registro_evaluado, uint32_t nueva_instruccion) {
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
    //TODO
    log_debug(cpu->logger, "Ejecutando SYSCALL: %s", instruccion.nombre_operacion);
}