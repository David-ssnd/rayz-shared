#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <stdint.h>
#include "photodiode.hpp"

extern Photodiode photodiode;

extern uint32_t expectedMessage;
extern bool hasExpectedMessage;
extern TickType_t last_expected_update;
extern uint8_t last_shooter_hash;

extern uint16_t all_expected_messages;
extern uint16_t correct_messages;
extern uint16_t not_expected_messages;

extern QueueHandle_t photodiodeMessageQueue;
extern SemaphoreHandle_t statsMutex;

bool init_task_shared();
