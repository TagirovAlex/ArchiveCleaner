#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <string>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>

class Mailer {
public:
    Mailer() = default;
    ~Mailer();

    bool connect(const std::string& server, int port);
    bool sendMail(
        const std::string& senderName,
        const std::string& senderEmail,
        const std::vector<std::string>& recipients,
        const std::string& subject,
        const std::string& htmlBody,
        bool useAuth = false,
        const std::string& username = "",
        const std::string& password = ""
    );
    void disconnect();

private:
    bool readResponse(std::string& response);
    bool sendCommand(const std::string& cmd, const std::string& expectedCode, std::string* outResponse = nullptr);
    std::string base64Encode(const std::string& input);
    std::string buildMessageId();

    SOCKET m_socket = INVALID_SOCKET;
    std::string m_server;
    int m_port = 0;
};
