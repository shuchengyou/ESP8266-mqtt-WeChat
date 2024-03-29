
#include <stddef.h>
#include "freertos/freertos.h"
#include "freertos/task.h"
#include "mqtt/MQTTClient.h"
#include "user_config.h"
#include "esp_common.h"

//缁撴瀯浣�
typedef struct esp_mqtt_msg_type {
	int power;
	int cw;
	int ww;
	int r;
	int g;
	int b;
	int workMode;
	int skill;
	char allData[1024];
} xMessage;



#define MQTT_CLIENT_THREAD_NAME         "mqtt_client_thread"
#define MQTT_CLIENT_THREAD_STACK_WORDS  2048
#define MQTT_CLIENT_THREAD_PRIO         8

LOCAL xTaskHandle mqttc_client_handle;
//MQTT接收msg queue
xQueueHandle MqttMessageQueueHandler;

#define Z_DEBUG()	printf("%s,%d\r\n",__func__,__LINE__);
static void messageArrived(MessageData* data)
{
    printf("Message arrived: %s\n", data->message->payload);
}

static void mqtt_client_thread(void* pvParameters)
{


	bool isNeedQueue = true;
	Network network;
	unsigned char sendbuf[2048], readbuf[2048] = { 0 };
	int rc = 0, count = 0;
	MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;
	pvParameters = 0;
	NetworkInit(&network);

	MQTTClient client;
	MQTTClientInit(&client, &network, 30000, sendbuf, sizeof(sendbuf), readbuf,
			sizeof(readbuf));

	for (;;) {

		while (wifi_station_get_connect_status() != STATION_GOT_IP) {
			vTaskDelay(1000 / portTICK_RATE_MS);
		}

		char* address = MQTT_BROKER; //服务器地址
		connectData.MQTTVersion = 3;
		connectData.clientID.cstring = CON_CLI_ID;
		connectData.username.cstring = CON_CLI_USERNAME;
		connectData.password.cstring = CON_CLI_PWD;
		connectData.keepAliveInterval = 40;
		connectData.cleansession = true;

		if ((rc = NetworkConnect(&network, address, MQTT_PORT)) != 0) {
			printf("MClouds NetworkConnect connect is %d\n", rc);
		}

		if ((rc = MQTTStartTask(&client)) != pdPASS) {
			printf("Return code from start tasks is %d\n", rc);
		} else {
			printf("Use MQTTStartTask\n");
		}

		if ((rc = MQTTConnect(&client, &connectData)) != 0) {
			printf("[SY] MClouds connect is %d\n", rc);
			network.disconnect(&network);
			vTaskDelay(1000 / portTICK_RATE_MS);
		}

		if ((rc = MQTTSubscribe(&client, MQTT_TOPIC_1, QOS0, messageArrived))
				!= 0) {
			printf("[SY] MClouds sub fail is %d\n", rc);
			network.disconnect(&network);
			vTaskDelay(1000 / portTICK_RATE_MS);
		}
		Z_DEBUG();

		//初始化队列
		MqttMessageQueueHandler = xQueueCreate( 10, sizeof( unsigned portLONG ));
     	if(!MqttMessageQueueHandler)
			return;
		
		xQueueReset(MqttMessageQueueHandler);
		Z_DEBUG();

		while (1) {

			char payload[2048];
			Z_DEBUG();

			if (isNeedQueue) {

				struct esp_mqtt_msg_type *pMsg;//接收的临时mesg
				printf("MqttMessageQueueHandler waitting ..\n");
				xQueueReceive(MqttMessageQueueHandler, &pMsg, portMAX_DELAY);
				sprintf(payload, "%s", pMsg->allData);

				os_printf(" [SY] 1 MQTT get freeHeap: %d\n",
						system_get_free_heap_size());
			} else {
				os_printf(" [SY] 2 MQTT get freeHeap: %d\n",
						system_get_free_heap_size());
			}
			Z_DEBUG();

			MQTTMessage message;
			message.qos = QOS0;
			message.retained = false;
			message.payload = (void*) payload;//小程序发来的消息，进行逻辑处理
			message.payloadlen = strlen(payload) + 1;

			if ((rc = MQTTPublish(&client, MQTT_TOPIC_1, &message)) != 0) {
				printf("Return code from MQTT publish is %d\n", rc);
			} else {
				printf("MQTT publish succeed ..\n");
			}
			Z_DEBUG();

			if (rc != 0) {
				isNeedQueue = false;
				break;
			} else {
				isNeedQueue = true;
			}

		}
		network.disconnect(&network);
	}

	printf("mqtt_client_thread going to be deleted\n");
	vTaskDelete(NULL);
	return;

}

void user_conn_init(void)
{
    int ret;
    ret = xTaskCreate(mqtt_client_thread,
                      MQTT_CLIENT_THREAD_NAME,
                      MQTT_CLIENT_THREAD_STACK_WORDS,
                      NULL,
                      MQTT_CLIENT_THREAD_PRIO,
                      &mqttc_client_handle);

    if (ret != pdPASS)  {
        printf("mqtt create client thread %s failed\n", MQTT_CLIENT_THREAD_NAME);
    }
}
