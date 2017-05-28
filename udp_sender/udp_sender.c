#include <string.h>

#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "lwip/api.h"
#include "ssid_config.h"
#include <math.h>

#define BUFFER_LENGTH 500
#define AVG_COUNT 10
#define MAX_VOTE_LENGTH 30

struct ABuffer
{
    char dataBytes[BUFFER_LENGTH];
    unsigned short is_full;
};

struct ABuffer myBuffer[2];
double pastAvg[AVG_COUNT];
size_t avgIndex = 0;
float incommingVote = 0;
unsigned short is_master = 0;

TaskHandle_t xStreamerHandle;

double avg(char *data)
{
    double sum = 0;

    for (int i = 0; i < BUFFER_LENGTH; ++i)
    {
        sum = sum + abs(data[i]-127);
    }

    return sum/BUFFER_LENGTH;

}

void addAvg(double newAvg)
{
    if (avgIndex < AVG_COUNT)
    {
        pastAvg[avgIndex] = newAvg;
        avgIndex++;
    }
    else
    {
        avgIndex = 0;
        pastAvg[avgIndex] = newAvg;
    }
}

double avg_avg(unsigned int n_avg)
{
    double sum = 0;

    for (int i = 0; i < n_avg; ++i)
    {
        sum = sum + pastAvg[i];
    }

    return sum/n_avg;
}

void broadcast_listener(void *pvParameters)
{

    err_t err;

    while(1) {
        struct netconn* conn;

        conn = netconn_new(NETCONN_UDP);

        // Connect to local port
        err = netconn_bind(conn, IP_ADDR_ANY, 34254);

        if (err != ERR_OK) {
            netconn_delete(conn);
            printf("%s : Could not bind! (%s)\n", __FUNCTION__, lwip_strerr(err));
            continue;
        }


        while(1) {

            struct netbuf *broadcast_buffer;
            err_t err;

            while( (err = netconn_recv(conn, &broadcast_buffer)) == ERR_OK ) {

                char *data;
                char msg[MAX_VOTE_LENGTH] = "";
                u16_t length = 0;

                netbuf_data(broadcast_buffer, (void**)&data, &length);

                if (length > MAX_VOTE_LENGTH)
                {
                    length = MAX_VOTE_LENGTH;
                }


                printf("Data length: %d\n", length);

                strncpy( msg, data, length );

                msg[MAX_VOTE_LENGTH-1] = '\0'; // Ensure null termination

                printf("Message is: %s\n", msg);

                float newVote = 0;
                int result = sscanf(msg, "vote:%f", &newVote);

                if (result == 1)
                {
                    printf("Got new vote value: %f\n", newVote);

                    if (newVote > incommingVote)
                    {
                    	incommingVote = newVote;
                    }
                }
                else
                {
                    printf("Unable to parse vote as starndard vote format.\n");
                }

                netbuf_free(broadcast_buffer);
            }

            netconn_close(conn);

            vTaskDelay(1000/portTICK_PERIOD_MS);

        }

        err = netconn_disconnect(conn);
        printf("%s : Disconnected from IP_ADDR_BROADCAST port 12346 (%s)\n", __FUNCTION__, lwip_strerr(err));

        err = netconn_delete(conn);
        printf("%s : Deleted connection (%s)\n", __FUNCTION__, lwip_strerr(err));

        vTaskDelay(1000/portTICK_PERIOD_MS);


    }

}


void voting_task(void *pvParameters)
{
    while(1) {
        vTaskDelay(2000/portTICK_PERIOD_MS);

        // Broadcaster part
        err_t err;

        // Send out some UDP data
        struct netconn* conn;

        // Create UDP connection
        conn = netconn_new(NETCONN_UDP);

        ip_addr_t server;
        ipaddr_aton("10.0.0.135", &server);

        err = netconn_connect(conn, &server, 34254);

        if (err != ERR_OK) {
            netconn_delete(conn);
            printf("%s : Could not connect! (%s)\n", __FUNCTION__, lwip_strerr(err));
            continue;
        }

        struct netbuf* buf = netbuf_new();
        void* data = netbuf_alloc(buf, MAX_VOTE_LENGTH);

        char vote[30] = "";
        float avgValue = (float) avg_avg(AVG_COUNT);
        sprintf(vote, "vote:%f", avgValue);

        strncpy(data, vote, MAX_VOTE_LENGTH);
        err = netconn_send(conn, buf);

        if (err != ERR_OK) {
            printf("%s : Could not send data!!! (%s)\n", __FUNCTION__, lwip_strerr(err));
            continue;
        }
        netbuf_delete(buf); // De-allocate packet buffer

        err = netconn_disconnect(conn);
        printf("%s : Disconnected from IP_ADDR_BROADCAST port 50000 (%s)\n", __FUNCTION__, lwip_strerr(err));

        err = netconn_delete(conn);
        printf("%s : Deleted connection (%s)\n", __FUNCTION__, lwip_strerr(err));


        // Wait for reply
        vTaskDelay(100/portTICK_PERIOD_MS);

        if (avgValue > incommingVote)
        {
            printf("Got vote majority. Starting stream!\n");
            vTaskResume(xStreamerHandle);
            is_master = 1;
            incommingVote = 0;
        }
        else
        {
            printf("No longer got vote majority. Stopping stream!\n");
            vTaskSuspend(xStreamerHandle);
            is_master = 0;
            incommingVote = 0;
        }


    }

}


void stream_audio(void *pvParameters)
{
    // Broadcaster part
    err_t err;

    while(1) {

        // Send out some UDP data
        struct netconn* conn;

        // Create UDP connection
        conn = netconn_new(NETCONN_UDP);

        ip_addr_t server;

    	ipaddr_aton("10.0.0.111", &server);


        err = netconn_connect(conn, &server, 51803);

        if (err != ERR_OK) {
            netconn_delete(conn);
            printf("%s : Could not connect! (%s)\n", __FUNCTION__, lwip_strerr(err));
            continue;
        }

        for(;;) {
            
            if (is_master == 1)
            {

                for (int i = 0; i < 2; ++i)
                {
                    if (myBuffer[i].is_full == 1)
                    {
                        printf("Detected full buffer. Sending data in buffer: %d\n", i);

                        printf("Avg: %f\n", avg_avg(AVG_COUNT));

                        struct netbuf* buf = netbuf_new();
                        void* data = netbuf_alloc(buf, BUFFER_LENGTH);

                        memcpy (data, myBuffer[i].dataBytes, BUFFER_LENGTH);
                        err = netconn_send(conn, buf);

                        if (err != ERR_OK) {
                            printf("%s : Could not send data!!! (%s)\n", __FUNCTION__, lwip_strerr(err));
                            continue;
                        }
                        netbuf_delete(buf); // De-allocate packet buffer
                        myBuffer[i].is_full = 0;
                    }
                }

            }
            else
            {
                printf("I am not master, so I stop streaming.\n");
            }
        }

        err = netconn_disconnect(conn);
        printf("%s : Disconnected from IP_ADDR_BROADCAST port 12346 (%s)\n", __FUNCTION__, lwip_strerr(err));

        err = netconn_delete(conn);
        printf("%s : Deleted connection (%s)\n", __FUNCTION__, lwip_strerr(err));

        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}

void read_UART_data(void *pvParameters)
{
    unsigned short activeBuffer = 0;
    unsigned int bufferIndex = 0;

    while(1) {
        
        char c = getchar();

        if (c != EOF)
        {
            myBuffer[activeBuffer].dataBytes[bufferIndex] = c;
            bufferIndex++;
            
            if (bufferIndex == BUFFER_LENGTH)
            {
                if (activeBuffer == 0)
                {
                    myBuffer[0].is_full = 1;
                    activeBuffer = 1;
                    addAvg(avg(myBuffer[0].dataBytes));
                }
                else {
                    myBuffer[1].is_full = 1;
                    activeBuffer = 0;
                    addAvg(avg(myBuffer[1].dataBytes));
                }    
                bufferIndex = 0;
                
            }
        }

    }
}

void user_init(void)
{
    memset(pastAvg, 0, sizeof(pastAvg)); // zero out Avg array

    uart_set_baud(0, 115200);

    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    // Set led to indicate wifi status.
    sdk_wifi_status_led_install(2, PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);

    struct sdk_station_config config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
    };

    // Required to call wifi_set_opmode before station_set_config.
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);

    xTaskCreate(&stream_audio, "stream_audio", 512, NULL, 2, &xStreamerHandle);
    xTaskCreate(&read_UART_data, "read_UART_data", 256, NULL, 2, NULL);
    xTaskCreate(&broadcast_listener, "broadcast_listener", 512, NULL, 2, NULL);
    xTaskCreate(&voting_task, "voting_task", 256, NULL, 2, NULL);
}

