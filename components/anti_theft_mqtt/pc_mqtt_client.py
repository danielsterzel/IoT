import paho.mqtt.client as mqtt

BROKER_IP = "172.20.10.10"
BROKER_PORT = 1883
USER_ID = "daniel"
DEVICE_ID = "device01"
TOPIC_BASE = f"anti_theft/{USER_ID}/{DEVICE_ID}/#"
TOPIC_COMMAND = f"anti_theft/{USER_ID}/{DEVICE_ID}/command"

print("MQTT OK")

def connection_handler(client, user_data, flags, result_code):
    print(f"Connected with result code:{result_code}")
    client.subscribe(TOPIC_BASE)
    print(f"Subscribed to: {TOPIC_BASE}")

    client.publish(TOPIC_COMMAND, "")

def message_handler(client, user_data, msg):
    print(f"[{msg.topic}] {msg.payload.decode('utf-8')}")

client = mqtt.Client()
client.on_connect = connection_handler
client.on_message = message_handler

client.connect(BROKER_IP, BROKER_PORT, 60)
client.loop_forever()