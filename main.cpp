#include "mbed.h"
#include "MQTTSocket.h"
#include "MQTTClient.h"
#include "NetworkInterface.h"
#include "string.h"
#include "cstring"


// WiFi credentials
const char* WIFI_SSID = "It was me, Dio!";
const char* WIFI_PASSWORD = "Muda!Muda!Muda!";

// MQTT broker and topic
const char* MQTT_BROKER = "m2m.eclipse.org";
const int MQTT_PORT = 1883;
const char* MQTT_TOPIC = "your/topic";

// UART configuration
RawSerial uart(PA_9, PA_10, 9600); // TX, RX, baud rate

WiFiInterface *wifi;

void messageArrived(MQTT::MessageData& md) {
    MQTT::Message &message = md.message;
    printf("Message arrived: %.*s\n", message.payloadlen, (char*)message.payload);
}

void parse_and_send_data(MQTT::Client<MQTTSocket, Countdown> &client, const char* data) {
    char instance_id[13];
    int rssi, angle1, angle2, reserved, channel, timestamp, sequence_number;
    char anchor_id[13], user_defined[50];

    sscanf(data, "+UUDF:%12[^,],%d,%d,%d,%d,%d,\"%12[^\"]\",\"%49[^\"]\",%d,%d",
           instance_id, &rssi, &angle1, &angle2, &reserved, &channel, anchor_id, user_defined, &timestamp, &sequence_number);

    char mqtt_message[256];
    snprintf(mqtt_message, sizeof(mqtt_message),
             "{\"instance_id\":\"%s\",\"rssi\":%d,\"angle1\":%d,\"angle2\":%d,\"channel\":%d,\"anchor_id\":\"%s\",\"timestamp\":%d,\"sequence_number\":%d}",
             instance_id, rssi, angle1, angle2, channel, anchor_id, timestamp, sequence_number);

    MQTT::Message message;
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)mqtt_message;
    message.payloadlen = strlen(mqtt_message);

    client.publish(MQTT_TOPIC, message);
}

int main() {
    printf("WiFi and MQTT example\n");

    wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
        printf("ERROR: No WiFiInterface found.\n");
        return -1;
    }

    printf("\nConnecting to %s...\n", WIFI_SSID);
    int ret = wifi->connect(WIFI_SSID, WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
        printf("\nConnection error: %d\n", ret);
        return -1;
    }

    printf("Connected to WiFi\n");
    printf("IP: %s\n", wifi->get_ip_address());

    MQTTSocket ipstack;
    MQTT::Client<MQTTSocket, Countdown> client(ipstack);

    printf("Connecting to %s:%d\n", MQTT_BROKER, MQTT_PORT);
    ret = ipstack.connect(MQTT_BROKER, MQTT_PORT);
    if (ret != 0) {
        printf("TCP connect failed: %d\n", ret);
        return -1;
    }

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = "mbed-sample";

    if ((ret = client.connect(data)) != 0) {
        printf("MQTT connect failed: %d\n", ret);
        return -1;
    }

    if ((ret = client.subscribe(MQTT_TOPIC, MQTT::QOS1, messageArrived)) != 0) {
        printf("MQTT subscribe failed: %d\n", ret);
        return -1;
    }

    char buf[256];
    while (true) {
        if (uart.readable()) {
            int i = 0;
            while (uart.readable() && i < sizeof(buf) - 1) {
                buf[i++] = uart.getc();
            }
            buf[i] = '\0'; // Null-terminate the string
            parse_and_send_data(client, buf);
        }
        client.yield(100);
    }

    if ((ret = client.unsubscribe(MQTT_TOPIC)) != 0) {
        printf("MQTT unsubscribe failed: %d\n", ret);
    }

    if ((ret = client.disconnect()) != 0) {
        printf("MQTT disconnect failed: %d\n", ret);
    }

    ipstack.disconnect();
    wifi->disconnect();
    printf("\nDone\n");
}