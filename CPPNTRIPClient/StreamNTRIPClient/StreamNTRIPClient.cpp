// StreamNTRIPClient.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
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

#define CS_SERVER_ADDRESS "192.168.1.114"
#define CS_SERVER_PORT "2101"
#define CS_MOUNT_POINT "U-BLOX"
#define CS_USERNAME "admin"
#define CS_PASSWORD "admin"

const int kBufferSize = 65536;
const char kCasterAgent[] = "NTRIP NTRIPCaster";
const char kClientAgent[] = "NTRIP NTRIPClient";
const char kServerAgent[] = "NTRIP NTRIPServer";

std::string kBase64CodeTable ="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

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
    // Generate request data format of ntrip.
   // 'Build request message
     //   Dim msg As String = "GET /" & NTRIPMountPoint & " HTTP/1.0" & vbCr & vbLf
     //   msg += "User-Agent: NTRIP LefebureNTRIPClient/20131124" & vbCr & vbLf
     //   msg += "Accept: */*" & vbCr & vbLf & "Connection: close" & vbCr & vbLf
     //   If NTRIPUsername.Length > 0 Then
     //   Dim auth As String = ToBase64(NTRIPUsername & ":" & NTRIPPassword)
     //   msg += "Authorization: Basic " & auth & vbCr & vbLf 'This line can be removed if no authorization is needed
     //  End If
     //  msg += vbCr & vbLf
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

    while (1) {

        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(socket_peer, &reads);
#if !defined(_WIN32)
        FD_SET(0, &reads);
#endif

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        if (select(socket_peer + 1, &reads, 0, 0, &timeout) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
            return 1;
        }

        if (FD_ISSET(socket_peer, &reads)) {
            char read[4096];
            int bytes_received = recv(socket_peer, read, 4096, 0);
            if (bytes_received < 1) {
                printf("Connection closed by peer.\n");
                break;
            }
            printf("Received (%d bytes): %.*s",
                bytes_received, bytes_received, read);
        }
#if defined(_WIN32)
        if (_kbhit()) {
#else
        if (FD_ISSET(0, &reads)) {
#endif
            char read[4096];
            if (!fgets(read, 4096, stdin)) break;
            if (read[0] == 'Q' || read[0] == 'q')
            {
                printf("Exit program\n");
                break;
            }
            
        }
    } //end while(1)

    printf("Closing socket...\n");
    CLOSESOCKET(socket_peer);

#if defined(_WIN32)
    WSACleanup();
#endif

    printf("Finished.\n");
    return 0;
}