/*
* SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: CC0-1.0
*/
 
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
 
int flag = 0;
 
typedef struct{
    int id;
    int random_number;
} robustez2_t;
 
// Recursos globais
QueueHandle_t fila = NULL;
EventGroupHandle_t event_supervisor = NULL;
 
// Bits de sinalização
#define BIT_TASK1_OK (1 << 0)
#define BIT_TASK2_OK (1 << 1)
 
void Task1 (void *pv)
{
    int seq = 0;
    for(;;)
    {
        robustez2_t *var = (robustez2_t *) malloc(sizeof(robustez2_t));
        if(var == NULL)
        {
            printf("{Leonardo Filiaci-RM89370} Não foi possível alocar dinamicamente\n");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
 
        var->id = seq++;
        var->random_number = +seq;
 
        if(xQueueSend(fila, &var, 0) != pdTRUE)
        {
            printf("{Leonardo Filiaci-RM89370} Valor não enviado\n");
            free(var);
        }
        else
        {
            // sinalizar para task 3 que a task 1 está operacional
            xEventGroupSetBits(event_supervisor, BIT_TASK1_OK);
            printf("{Leonardo Filiaci-RM89370} Valor %d de ID %d enviado com sucesso\n", var->random_number, var->id);
        }
 
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
 
void Task2(void *pv)
{
    robustez2_t *pointer = NULL;
    int timeout = 0;
    for(;;)
    {
        if(xQueueReceive(fila, &pointer, 0) == pdTRUE)
        {
            printf("{Leonardo Filiaci-RM89370} Valor %d de ID %d recebido com sucesso\n", pointer->random_number, pointer->id);
 
            timeout = 0;
            // reseta WDT
            esp_task_wdt_reset();
            // sinaliza para task 3
 
            xEventGroupSetBits(event_supervisor, BIT_TASK2_OK);
 
            // libera memória depois de usar
            if(pointer != NULL) {
                free(pointer);
                pointer = NULL;
            }
        }
        else
        {
            timeout++;
            printf("{Leonardo Filiaci-RM89370} Não foi possível receber valor\n");
        }
 
        if(timeout == 5)
        {
            printf("{Leonardo Filiaci-RM89370} Não foi possível receber valor da fila\n");
        }
        else if(timeout == 10)
        {
            printf("{Leonardo Filiaci-RM89370} Recuperação moderada - Limpa fila\n");
            xQueueReset(fila);
        }
        else if (timeout == 15)
        {
            printf("{Leonardo Filiaci-RM89370} Recuperação agressiva - Resetar o sistema\n");
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
        }
 
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
 
void Task3 (void *pv)
{
    for(;;)
    {
        // Aguarda por até 2 segundos sinais das tasks 1 e 2
        EventBits_t bits = xEventGroupWaitBits(
            event_supervisor,
            BIT_TASK1_OK | BIT_TASK2_OK,
            pdTRUE, // limpa bits após leitura
            pdFALSE, // qualquer bit serve
            pdMS_TO_TICKS(0)
        );
 
        if((bits & BIT_TASK1_OK) && (bits & BIT_TASK2_OK))
        {
            printf("{Leonardo Filiaci-RM89370} Sistema OK (Task1 e Task2 ativas)\n");
        }
        else if(bits & BIT_TASK1_OK)
        {
            printf("{Leonardo Filiaci-RM89370} Sistema parcialmente OK (apenas Task1)\n");
        }
        else if(bits & BIT_TASK2_OK)
        {
            printf("{Leonardo Filiaci-RM89370} Sistema parcialmente OK (apenas Task2)\n");
        }
        else
        {
            printf("{Leonardo Filiaci-RM89370} Sistema com falha (nenhuma task sinalizou)\n");
        }
        flag = 0;
 
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_task_wdt_reset();
    }
}
 
void app_main(void)
{
    // Configura o WDT
    esp_task_wdt_config_t wdt = {
        .timeout_ms = 5000,
        .idle_core_mask = (1 << 0) | (1 << 1),
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt);
 
    // Cria fila para ponteiros de robustez2_t
    fila = xQueueCreate(1, sizeof(robustez2_t*));
    // Cria grupo de eventos
    event_supervisor = xEventGroupCreate();
 
    if(fila == NULL || event_supervisor == NULL)
    {
        printf("{Leonardo Filiaci-RM89370} Falha na criação da fila ou do event group\n");
        esp_restart();
    }
 
    TaskHandle_t hTask1, hTask2, hTask3;
 
    xTaskCreate(Task1, "Task1", 8192, NULL, 5, &hTask1);
    xTaskCreate(Task2, "Task2", 8192, NULL, 5, &hTask2);
    xTaskCreate(Task3, "Task3", 8192, NULL, 5, &hTask3);
 
    // // Adiciona tasks ao WDT
    esp_task_wdt_add(hTask1);
    esp_task_wdt_add(hTask2);
    esp_task_wdt_add(hTask3);
}