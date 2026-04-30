// StreamNTRIPClient.cpp : This file contains the 'main' function. Program execution begins and ends there.
// https://dzone.com/articles/parallel-tcpip-socket-server-with-multi-threading
#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#endif


#if defined(_WIN32)
#define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
#define CLOSESOCKET(s) closesocket(s)
#define GETSOCKETERRNO() (WSAGetLastError())

#else
#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)
#endif

#if defined(_WIN32)
#include <conio.h>
#endif

#include <stdio.h>
#include <string.h>
#include <iostream>

#include <thread>
#include <mutex>
#include <condition_variable>

#include "ringbuffer.h"
/// MQTT
#include <cassert>
#include "MQTTClient.h"
#define CS_MQTT_URL "mqtt://broker.freemqtt.com:1883"
#define CS_MQTT_USER_ID "HOME_HN"
#define CS_MQTT_USER_NAME "freemqtt"
#define CS_MQTT_PASSWORD "public"
#define CS_MQTT_TOPIC_RTCM "RTCM"
#define CS_MQTT_TOPIC_RTCM_SYS "RTCM\SYS"
MQTTClient mqttClient;
char mqttTopic[255];


#define CS_FRAME_SEND 128
#define CS_SERVER_ADDRESS "192.168.1.114"
#define CS_SERVER_PORT "2101"
#define CS_MOUNT_POINT "U-BLOX"
#define CS_USERNAME "admin"
#define CS_PASSWORD "admin"
#define CS_MAX_BUFF 4096

const int kBufferSize = 65536;
const char kCasterAgent[] = "NTRIP NTRIPCaster";
const char kClientAgent[] = "NTRIP NTRIPClient";
const char kServerAgent[] = "NTRIP NTRIPServer";

std::string kBase64CodeTable ="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::mutex mtx;
std::condition_variable cv;
RingBuffer bufferRead;
bool gExit=0;

inline int base64_index(char in) {
    return kBase64CodeTable.find(in);
}

int Base64Encode(std::string const& raw, std::string* out) {
    int len = raw.size();
    for (int i = 0; i < len; i += 3) {
        out->push_back(kBase64CodeTable[(raw[i] & 0xFC) >> 2]);
        if (i + 1 >= len) {
            out->push_back(kBase64CodeTable[(raw[i] & 0x03) << 4]);
            break;
        }
        out->push_back(kBase64CodeTable[(raw[i] & 0x03) << 4 | (raw[i + 1] & 0xF0) >> 4]);
        if (i + 2 >= len) {
            out->push_back(kBase64CodeTable[(raw[i + 1] & 0x0F) << 2]);
            break;
        }
        else {
            out->push_back(kBase64CodeTable[(raw[i + 1] & 0x0F) << 2 | (raw[i + 2] & 0xC0) >> 6]);
        }
        out->push_back(kBase64CodeTable[raw[i + 2] & 0x3F]);
    }
    len = out->size();
    if (len % 4 != 0) {
        out->append(std::string(4 - len % 4, '='));
    }
    return 0;
}

int Base64Decode(std::string const& raw, std::string* out) {
    if (out == nullptr) return -1;
    int len = raw.size();
    if ((len == 0) || (len % 4 != 0)) return -1;
    out->clear();
    for (int i = 0; i < len; i += 4) {
        out->push_back(((base64_index(raw[i]) & 0x3F) << 2) |
            ((base64_index(raw[i + 1]) & 0x3F) >> 4));
        if (raw[i + 2] == '=') {
            out->push_back(((base64_index(raw[i + 1]) & 0x0F) << 4));
            break;
        }
        out->push_back(((base64_index(raw[i + 1]) & 0x0F) << 4) |
            ((base64_index(raw[i + 2]) & 0x3F) >> 2));
        if (raw[i + 3] == '=') {
            out->push_back(((base64_index(raw[i + 2]) & 0x03) << 6));
            break;
        }
        out->push_back(((base64_index(raw[i + 2]) & 0x03) << 6) |
            (base64_index(raw[i + 3]) & 0x3F));
    }
    return 0;
}


bool SendNTRIPUser(SOCKET socket_peer,std::string user,std::string passwd)
{
    // Ntrip connection authentication.
    int ret = -1;
    std::string user_passwd = user + ":" + passwd;
    std::string user_passwd_base64;
    std::unique_ptr<char[]> buffer(
        new char[kBufferSize], std::default_delete<char[]>());
    // Generate base64 encoding of username and password.
    Base64Encode(user_passwd, &user_passwd_base64);
    // Generate request data format of ntrip. Build request message     
    if (user.empty())
        ret = snprintf(buffer.get(), kBufferSize - 1,
            "GET /%s HTTP/1.1\r\n"
            "User-Agent: %s\r\n"            
            "Accept: */*\r\nConnection: close\r\n"
            "\r\n",
            CS_MOUNT_POINT, kClientAgent);
    else
        ret = snprintf(buffer.get(), kBufferSize - 1,
            "GET /%s HTTP/1.1\r\n"
            "User-Agent: %s\r\n"        
            "Authorization: Basic %s\r\n"         
            "Accept: */*\r\nConnection: close\r\n"
            "\r\n",
            CS_MOUNT_POINT, kClientAgent, user_passwd_base64.c_str());

    if (send(socket_peer, buffer.get(), ret, 0) < 0) {
        printf("Send request failed!!!\r\n");
#if defined(WIN32) || defined(_WIN32)
        closesocket(socket_peer);
        WSACleanup();
#else
        close(socket_fd);
#endif  // defined(WIN32) || defined(_WIN32)
        return false;
    }
    return true;
}
void MQTT_Clear(MQTTClient c, const char* clientID,const char* clientUser, const char* clientPass,int nVesion= MQTTVERSION_DEFAULT)
{
    int rc;
    printf("Stopping\n");
    rc = MQTTClient_unsubscribe(c, mqttTopic);
    assert("Unsubscribe successful", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
    rc = MQTTClient_disconnect(c, 0);
    assert("Disconnect successful", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

    // Just to make sure we can connect again 
    MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;
    MQTTClient_willOptions wopts = MQTTClient_willOptions_initializer;
    opts.keepAliveInterval = 20;
    opts.cleansession = 1;
    opts.username = clientUser;
    opts.password = clientPass;
    opts.MQTTVersion = nVesion;
    char pbuff[255];
    sprintf_s(pbuff,255, "%s disconected", clientID);
    opts.will = &wopts;
    opts.will->message = pbuff;
    opts.will->qos = 1;
    opts.will->retained = 0;
    opts.will->topicName = CS_MQTT_TOPIC_RTCM_SYS;
    opts.will = NULL;

    rc = MQTTClient_connect(c, &opts);
    assert("Connect successful", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
    rc = MQTTClient_disconnect(c, 0);
    assert("Disconnect successful", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

    MQTTClient_destroy(&c);
}

MQTTClient MQTT_Connec(const char* serverURI,const char* clientID,const char* clientUser,const char* clientPass,const int nVesion= MQTTVERSION_DEFAULT)
{
    int subsqos = 2;
    MQTTClient c;
    MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;
    MQTTClient_willOptions wopts = MQTTClient_willOptions_initializer;
    int rc = 0;
    

    rc = MQTTClient_create(&c, serverURI, clientID,MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);
    assert("good rc from create", rc == MQTTCLIENT_SUCCESS, "rc was %d\n", rc);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        MQTTClient_destroy(&c);
        goto exit;
    }
    
    opts.keepAliveInterval = 20;
    opts.cleansession = 1;
    opts.username = clientUser;
    opts.password = clientPass;
    opts.MQTTVersion = nVesion;
    char pbuff[255];
    sprintf_s(pbuff,255, "%s disconected", clientID);
    opts.will = &wopts;
    opts.will->message = pbuff;
    opts.will->qos = 1;
    opts.will->retained = 0;
    opts.will->topicName = CS_MQTT_TOPIC_RTCM_SYS;
    opts.will = NULL;

    printf("Connecting to MQTT broker");
    rc = MQTTClient_connect(c, &opts);
    assert("Good rc from connect", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
    if (rc != MQTTCLIENT_SUCCESS)
        goto exit;

    
    sprintf_s(mqttTopic, 255, "%s/%s", CS_MQTT_TOPIC_RTCM,clientID);
    rc = MQTTClient_subscribe(c, mqttTopic, subsqos);
    assert("Good rc from subscribe", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        printf("Stopping\n");
        //rc = MQTTClient_unsubscribe(c, test_topic);
        //assert("Unsubscribe successful", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
        rc = MQTTClient_disconnect(c, 0);
        assert("Disconnect successful", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

        // Just to make sure we can connect again 
        rc = MQTTClient_connect(c, &opts);
        assert("Connect successful", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
        rc = MQTTClient_disconnect(c, 0);
        assert("Disconnect successful", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

        MQTTClient_destroy(&c);
        return NULL;
    }
    else
        return c;    
exit:
    return NULL;
}
void MQTT_send(MQTTClient c, int qos, const char* topicName,void* data,const int len)
{
    MQTTClient_deliveryToken dt;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;       
    int rc;
    pubmsg.payload = data;
    pubmsg.payloadlen = len;
    pubmsg.qos = qos;
    pubmsg.retained = 0;
    rc = MQTTClient_publishMessage(c, topicName, &pubmsg, &dt);
    assert("Good rc from publish", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
    if (qos > 0)
    {
        rc = MQTTClient_waitForCompletion(c, dt, 5000L);
        assert("Good rc from waitforCompletion", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
    }
}
void SendFunction()
{
    uint8_t buffSend[CS_FRAME_SEND];
    uint16_t bytes_send;
    while(!gExit)
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [] { return ((RingBuffer_GetDataLength(&bufferRead)>= CS_FRAME_SEND) || (!gExit)); });
        while (RingBuffer_GetDataLength(&bufferRead) >= CS_FRAME_SEND) 
        {
            bytes_send=RingBuffer_Read(&bufferRead, buffSend, CS_FRAME_SEND);            
            
            printf("\r\nSend data (%d bytes)", bytes_send);
            if (mqttClient && (bytes_send > 0))
               MQTT_send(mqttClient, 0, mqttTopic, buffSend, bytes_send);
        }        
    }
}
int main()
{
#if defined(_WIN32)
    WSADATA d;
    if (WSAStartup(MAKEWORD(2, 2), &d)) {
        fprintf(stderr, "Failed to initialize.\n");
        return 1;
    }
#endif

    printf("Configuring remote address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* peer_address;
    if (getaddrinfo(CS_SERVER_ADDRESS, CS_SERVER_PORT, &hints, &peer_address)) {
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }


    printf("Remote address is: ");
    char address_buffer[100];
    char service_buffer[100];
    getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen,
        address_buffer, sizeof(address_buffer),
        service_buffer, sizeof(service_buffer),
        NI_NUMERICHOST);
    printf("%s %s\n", address_buffer, service_buffer);


    printf("Creating socket...\n");
    SOCKET socket_peer;
    socket_peer = socket(peer_address->ai_family,
        peer_address->ai_socktype, peer_address->ai_protocol);
    if (!ISVALIDSOCKET(socket_peer)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }


    printf("Connecting...\n");
    if (connect(socket_peer,
        peer_address->ai_addr, peer_address->ai_addrlen)) {
        fprintf(stderr, "connect() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }
    freeaddrinfo(peer_address);

    printf("Connected.\n");
    printf("To exit program , enter q/Q followed by enter.\n");

    if (!SendNTRIPUser(socket_peer, CS_USERNAME, CS_PASSWORD))
    {
        fprintf(stderr, "Send request UserName/Password to NTRIP caster failed!!!\r\n");
        return 1;
    }
    // init ringbufer read default 4096 bytes
    
    uint8_t buffSend[CS_FRAME_SEND];
    char readNet[CS_MAX_BUFF], readKey[CS_MAX_BUFF];
    uint16_t bytes_received, bytes_send;
    struct timeval timeout;
    fd_set reads;

    RingBuffer_Init(&bufferRead);
    gExit = 0;
    std::thread tSend(SendFunction);
    // connect to MQTT
    mqttClient = MQTT_Connec(CS_MQTT_URL, CS_MQTT_USER_ID, CS_MQTT_USER_NAME, CS_MQTT_PASSWORD);
    if (mqttClient == NULL)
        printf("Failed to initialize MQTT Client\n");
    else
        printf("Connected to MQTT Broker.\n");
   
    while (1) {    
        FD_ZERO(&reads);
        FD_SET(socket_peer, &reads);
#if !defined(_WIN32)
        FD_SET(0, &reads);
#endif
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        if (select(socket_peer + 1, &reads, 0, 0, &timeout) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());            
            return 1;
        }

        if (FD_ISSET(socket_peer, &reads)) {
           
            bytes_received = recv(socket_peer, readNet, CS_MAX_BUFF, 0);
            if (bytes_received < 1) {
                printf("Connection closed by peer.\n");                
                break;
            }
            //printf("Received (%d bytes): %.*s",bytes_received, bytes_received, read);
            printf("\r\nReceived (%d bytes)\t%d", bytes_received, bytes_received);

            {
                std::lock_guard<std::mutex> lock(mtx);
                if (RingBuffer_Write(&bufferRead, (uint8_t*)readNet, bytes_received) != RING_BUFFER_OK)
                    printf("\r\nWrite RingBuffer failed.");
            }          
            cv.notify_one();               
        }
#if defined(_WIN32)
        if (_kbhit()) {
#else
        if (FD_ISSET(0, &reads)) {
#endif
            if (!fgets(readKey, CS_MAX_BUFF, stdin)) break;
            if (readKey[0] == 'Q' || readKey[0] == 'q')
            {
                printf("Exit program\n");               
                break;
            }
            
        }
    } //end while(1)

    gExit = 1;
    tSend.join();

    printf("Closing socket...\n");
    CLOSESOCKET(socket_peer);
    
#if defined(_WIN32)
    WSACleanup();
#endif
    if (mqttClient != NULL)
        MQTT_Clear(mqttClient, CS_MQTT_USER_ID, CS_MQTT_USER_NAME, CS_MQTT_PASSWORD);
    printf("Finished.\n");
    return 0;
}