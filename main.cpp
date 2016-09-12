//----------------------------------------------------------------------------
//   The confidential and proprietary information contained in this file may
//   only be used by a person authorised under and to the extent permitted
//   by a subsisting licensing agreement from ARM Limited or its affiliates.
//
//          (C) COPYRIGHT 2016 ARM Limited or its affiliates.
//              ALL RIGHTS RESERVED
//
//   This entire notice must be reproduced on all copies of this file
//   and copies of this file may only be made by a person if such person is
//   permitted to do so under the terms of a subsisting license agreement
//   from ARM Limited or its affiliates.
//----------------------------------------------------------------------------

#include <mbed.h>
#include <rtos.h>

#include <EthernetInterface.h>

#define VERBOSE_DEBUG 0

EthernetInterface eth;
TCPSocket* tcp;
Serial output(USBTX, USBRX);

const char request[] = "GET /lootbox/firmware/alice.txt HTTP/1.1\r\nHost: s3-us-west-2.amazonaws.com\r\nRange: bytes=%d-%d\r\n\r\n";

char buffer[2000];

typedef enum {
    EVENT_CONNECT_DONE,
    EVENT_SEND_DONE,
    EVENT_RECEIVE_RETRY
} event_t;

volatile bool event_fired;
event_t event_expected;
int bytes = 0;
int offset = 10000;

void callback()
{
    event_fired = true;
}

void send_request()
{
    bytes = 0;
    offset += 1000;

    // create HTTP request
    int length = snprintf(buffer, sizeof(buffer), request, offset, offset + 1000);

    // send request
    event_expected = EVENT_SEND_DONE;
    int result = tcp->send(buffer, length);

    buffer[length] = '\0';
    printf("============\r\n");
    printf("HTTP request:\r\n%s", buffer);
    printf("send result: %d\r\n", result);
}

void receive_response()
{
    int result = tcp->recv(buffer, 1000);

    if (result > 0)
    {
        // accumulate received bytes
        bytes += result;

        // transfer ends when ~1400 bytes have been received
        if (bytes > 1000)
        {
            printf("receive done\r\n");

#if VERBOSE_DEBUG
            for (int index = 0; index < bytes; index++)
            {
                printf("%c", buffer[index]);
            }
            printf("\r\n");
#endif

            // start next transfer
            send_request();
        }
    }
    else
    {
        printf("nsapi receive error: %d\r\n", result);
    }
}

int main()
{
    output.baud(115200);
    output.printf("Using Ethernet\r\n");

    eth.connect();
    output.printf("IP address %s\r\n", eth.get_ip_address());

    tcp = new TCPSocket(&eth);

    tcp->set_blocking(false);
    tcp->attach(callback);

    event_expected = EVENT_CONNECT_DONE;
    tcp->connect("s3-us-west-2.amazonaws.com", 80);

    for (;;)
    {
        if (event_fired)
        {
            event_fired = false;
            int result;

            switch (event_expected)
            {
                case EVENT_CONNECT_DONE:
                    printf("connect done\r\n");
                    send_request();
                    break;

                case EVENT_SEND_DONE:
                    printf("send done\r\n");
                    event_expected = EVENT_RECEIVE_RETRY;

                    // fall through

                case EVENT_RECEIVE_RETRY:
                    receive_response();
                    break;

                default:
                    printf("unexpected event\r\n");
                    break;
            }
        }
        __WFI();
    }
}
