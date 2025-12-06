/* pc_mqtt_client.c
   PC mosquitto client that subscribes to smartsec/# and logs alarms.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mosquitto.h>
#include <time.h>

void on_connect(struct mosquitto *mosq, void *obj, int rc) {
    if (rc == 0) {
        printf("[PC MQTT] Connected. Subscribing to smartsec/#\n");
        mosquitto_subscribe(mosq, NULL, "smartsec/#", 0);
    } else {
        printf("[PC MQTT] Connect failed rc=%d\n", rc);
    }
}

void log_line(const char* line) {
    FILE *f = fopen("alarm_log.txt","a");
    if (!f) return;
    fprintf(f,"%s\n",line);
    fclose(f);
}

void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg) {
    time_t now = time(NULL);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));
    printf("[%s] Topic: %s | Message: %.*s\n", ts, msg->topic, msg->payloadlen, (char*)msg->payload);

    // quick parse: smartsec/<user>/<type>/<id>/<kind>
    char topic_copy[256];
    strncpy(topic_copy, msg->topic, sizeof(topic_copy)-1);
    topic_copy[sizeof(topic_copy)-1] = 0;
    char *parts[8];
    int i=0;
    char *tok = strtok(topic_copy,"/");
    while(tok && i<8){ parts[i++] = tok; tok = strtok(NULL,"/"); }

    if (i>=5) {
        const char* device_id = parts[3];
        const char* kind = parts[4];
        char payload[256]; memset(payload,0,sizeof(payload));
        memcpy(payload, msg->payload, msg->payloadlen);
        if (strcmp(kind,"sensor")==0) {
            if (strstr(payload,"trigger") || strcmp(payload,"open")==0) {
                char line[512];
                snprintf(line,sizeof(line), "%s ALERT device=%s payload=%s", ts, device_id, payload);
                log_line(line);
                printf(" -> logged alert\n");
            }
        }
    }
}

int main(int argc, char** argv) {
    const char* host = "127.0.0.1";
    int port = 1883;
    if (argc>=2) host = argv[1];
    if (argc>=3) port = atoi(argv[2]);

    mosquitto_lib_init();
    struct mosquitto *mosq = mosquitto_new(NULL, true, NULL);
    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_message_callback_set(mosq, on_message);

    if (mosquitto_connect(mosq, host, port, 60) != MOSQ_ERR_SUCCESS) {
        printf("Could not connect to broker %s:%d\n", host, port);
        return 1;
    }

    printf("PC MQTT client running...\n");
    mosquitto_loop_forever(mosq, -1, 1);

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}
