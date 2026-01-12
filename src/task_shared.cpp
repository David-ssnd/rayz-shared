#include "task_shared.h"

Photodiode photodiode;

uint32_t expectedMessage = 0;
bool hasExpectedMessage = false;
TickType_t last_expected_update = 0;
uint8_t last_shooter_hash = 0;

uint16_t all_expected_messages = 0;
uint16_t correct_messages = 0;
uint16_t not_expected_messages = 0;

QueueHandle_t photodiodeMessageQueue = nullptr;
SemaphoreHandle_t statsMutex = nullptr;

bool init_task_shared()
{
    photodiodeMessageQueue = xQueueCreate(10, sizeof(uint32_t));
    statsMutex = xSemaphoreCreateMutex();
    return photodiodeMessageQueue && statsMutex;
}
