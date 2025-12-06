#include <stddef.h>
#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#define TOPIC_BUFFER_SIZE 128

typedef struct {
    char userId[32];
    char deviceId[32];
    char topicCommand[TOPIC_BUFFER_SIZE];
    char topicStatus[TOPIC_BUFFER_SIZE];
    char topicEvent[TOPIC_BUFFER_SIZE];
    char topicDebug[TOPIC_BUFFER_SIZE];
} AntiTheftMqttTopics;

void antiTheftMqttStart();
void antiTheftTopicsInit(AntiTheftMqttTopics* t, const char* user, const char* device);
const char* antiTheftBuildTopic(const AntiTheftMqttTopics* t,  
    char* buffer, size_t bufferSize,
    const char* category,
    const char* subcategory);

#endif // MQTT_CLIENT_H