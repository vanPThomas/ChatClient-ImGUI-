#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <iostream>
#include <cstring>
#include <thread>
#include <fstream>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <cstdlib>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

void handleSystemCallError(std::string errorMsg);
int createClientSocket(const std::string &serverIP, int serverPort);
void receiveMessages(int clientSocket, int bufferSize, std::string password, std::vector<std::string> &chatMessages, char *buffer);
void sendMessage(int clientSocket, char outMessage[1024], int bufferSize, const std::string &username, std::string password);
std::string decryptData(const std::string &data, const std::string &key);
std::string encryptData(const std::string &data, const std::string &key);
void connectToServerThread(const std::string &IP, int port, std::vector<std::string> &chatMessages, int &clientSocket, char *buffer);
const int bufferSize = 10240;

void AddMessage(std::vector<std::string> &messages, const char *message)
{
    messages.push_back(message);
}

// Main code
int main(int, char **)
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }
#endif

    char buffer[bufferSize];
    char outMessage[bufferSize];
    int clientSocket;

    if (!glfwInit())
        return 1;

    // Create a GLFW window
    GLFWwindow *window = glfwCreateWindow(1280, 720, "ImGui Example", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // Load custom font (must stay alive/loaded while using it)
    ImFont *customFont = io.Fonts->AddFontFromFileTTF("fonts/Px437_IBM_VGA_8x14.ttf", 18.0f);
    if (!customFont)
    {
        return 1;
    }

    std::vector<std::string> chatMessages; // Store chat messages
    char inputBuffer[1024] = "";           // Buffer for input text
    char IP[20] = "";
    char Port[10] = "";
    char User[50] = "";
    char Password[1000] = "";
    bool focusInput = false; // Flag to set keyboard focus
    bool isConnected = false;

    // std::thread first(receiveMessages, clientSocket, bufferSize, Password, chatMessages, buffer);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::PushFont(customFont);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.5f, 1.0f)); // Red color
        {
            const float labelWidth = 220.0f;
            ImGui::Begin("Connection Data", NULL, ImGuiWindowFlags_None);
            ImGui::Text("IP: ");
            ImGui::SameLine(labelWidth);
            ImGui::InputText("##IP", IP, IM_ARRAYSIZE(IP), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::Text("Port: ");
            ImGui::SameLine(labelWidth);
            ImGui::InputText("##PORT", Port, IM_ARRAYSIZE(Port), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::Text("User Name: ");
            ImGui::SameLine(labelWidth);
            ImGui::InputText("##USERNAME", User, IM_ARRAYSIZE(User), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::Text("Encryption Password: ");
            ImGui::SameLine(labelWidth);
            ImGui::InputText("##ENCRYPTIONPASSWORD", Password, IM_ARRAYSIZE(Password), ImGuiInputTextFlags_EnterReturnsTrue);
            if (!isConnected)
            {
                if (ImGui::Button("Connect"))
                {
                    int PortNumber = std::atoi(Port);
                    std::string ipString = std::string(IP);             // Convert char array to std::string
                    std::string passwordString = std::string(Password); // Convert password to std::string

                    // Pass all the required arguments including passwordString
                    std::thread connectionThread(connectToServerThread, ipString, PortNumber, std::ref(chatMessages), std::ref(clientSocket), buffer);
                    connectionThread.detach(); // Detach the thread
                    isConnected = true;
                }
            }

            ImGui::End();
        }
        {
            // Chat window
            ImGui::Begin("Chat Window", NULL, ImGuiWindowFlags_None);

            // Display the chat log in a scrollable area
            ImGui::BeginChild("ChatArea", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);
            for (const auto &message : chatMessages)
            {
                ImGui::TextUnformatted(message.c_str());
            }
            ImGui::SetScrollHereY(1.0f); // Scroll to bottom
            ImGui::EndChild();

            // Input box for typing new messages
            if (focusInput)
            {
                ImGui::SetKeyboardFocusHere(); // Set focus to the input box after message sent
                focusInput = false;            // Reset focus flag
            }
            // ImGui::SetKeyboardFocusHere();
            ImGui::InputText("##Input", inputBuffer, IM_ARRAYSIZE(inputBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            // Send button or pressing Enter submits the message

            if (ImGui::Button("Send") || ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter)))
            {
                if (strlen(inputBuffer) > 0)
                {
                    // Add the input text as a new chat message
                    AddMessage(chatMessages, inputBuffer);
                    std::string userMessage;
                    std::string userString = std::string(User);
                    std::string passwordString = std::string(Password);

                    userMessage = userString + ": " + userMessage;

                    std::string encryptedData = encryptData(userMessage, passwordString); // Create a string with the received data
                    send(clientSocket, encryptedData.c_str(), encryptedData.length() + 1, 0);
                    // Clear the input buffer
                    inputBuffer[0] = '\0';
                    focusInput = true;
                }
            }

            ImGui::End();
        }

        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.0f, 0.0f, 0.0f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // first.join();

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

void handleSystemCallError(std::string errorMsg)
{
#ifdef _WIN32
    std::cout << errorMsg << ", WSA error code: " << WSAGetLastError() << "\n";
#else
    std::cout << errorMsg << ", errno: " << errno << "\n";
#endif
    exit(EXIT_FAILURE);
}

// creates a client socket
int createClientSocket(const std::string &serverIP, int serverPort)
{
#ifdef _WIN32
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET)
    {
        handleSystemCallError("Failed to create socket");
    }
#else
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1)
    {
        handleSystemCallError("Failed to create socket");
    }
#endif

    // Set up the server address structure
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);

    // check IP validity by converting it to binary
    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddress.sin_addr) <= 0)
    {
        handleSystemCallError("Invalid address or address not supported\n");
#ifdef _WIN32
        closesocket(clientSocket);
#else
        close(clientSocket);
#endif
        exit(EXIT_FAILURE);
    }

    // connect to server
    if (connect(clientSocket, reinterpret_cast<struct sockaddr *>(&serverAddress), sizeof(serverAddress)) == -1)
    {
        handleSystemCallError("Error when connecting to server\n");
#ifdef _WIN32
        closesocket(clientSocket);
#else
        close(clientSocket);
#endif
        exit(EXIT_FAILURE);
    }

    return clientSocket;
}

void receiveMessages(int clientSocket, int bufferSize, std::string password, std::vector<std::string> &chatMessages, char *buffer)
{
    char receiveBuffer[10240];

    while (true)
    {
        int bytesRead = recv(clientSocket, receiveBuffer, bufferSize - 1, 0);

        if (bytesRead > 0)
        {
            receiveBuffer[bytesRead] = '\0';
            std::string decryptedData = decryptData(receiveBuffer, password);
            AddMessage(chatMessages, decryptedData.c_str()); // Safely add received messages
        }
    }
}

// send messages to server
void sendMessage(int clientSocket, char outMessage[bufferSize], int bufferSize, const std::string &username, std::string password)
{
    while (true)
    {
        std::string userMessage;
        std::getline(std::cin, userMessage);

        if (userMessage.length() > bufferSize - username.length() - 3) // Considering space for ": " and null terminator
        {
            std::cout << "Warning: Message is too long. Please keep it within " << bufferSize - username.length() - 3 << " characters.\n";
            continue;
        }
        if (!userMessage.empty())
        {
            userMessage = username + ": " + userMessage;

            std::string encryptedData = encryptData(userMessage, password); // Create a string with the received data
            std::cout << encryptedData.size() << "\n";

            send(clientSocket, encryptedData.c_str(), encryptedData.length() + 1, 0);
        }
    }
}

std::string decryptData(const std::string &data, const std::string &password)
{
    // Generate a 32-byte key from the password using SHA256
    unsigned char key[32];
    SHA256(reinterpret_cast<const unsigned char *>(password.c_str()), password.length(), key);

    // Use the first 16 bytes of the key as the IV (for testing purposes)
    std::string iv(reinterpret_cast<char *>(key), 16);

    // Set up the OpenSSL decryption context
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_CIPHER_CTX_init(ctx);

    // Initialize the decryption operation with AES 256 CBC and the generated key and IV
    if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, reinterpret_cast<const unsigned char *>(iv.c_str())))
    {
        std::cerr << "Error initializing decryption\n";
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }

    int len;
    int plaintext_len;
    unsigned char *plaintext = new unsigned char[data.length() + EVP_CIPHER_block_size(EVP_aes_256_cbc())];

    // Perform the decryption
    if (!EVP_DecryptUpdate(ctx, plaintext, &len, reinterpret_cast<const unsigned char *>(data.c_str()), data.length()))
    {
        std::cerr << "Error during decryption\n";
        EVP_CIPHER_CTX_free(ctx);
        delete[] plaintext;
        return "";
    }
    plaintext_len = len;

    // Finalize the decryption
    if (EVP_DecryptFinal_ex(ctx, plaintext + len, &len) <= 0)
    {
        std::cerr << "Error finalizing decryption\n";
        EVP_CIPHER_CTX_free(ctx);
        delete[] plaintext;
        return "";
    }
    plaintext_len += len;

    // Clean up the context
    EVP_CIPHER_CTX_free(ctx);

    // Convert the decrypted data to a string
    std::string decryptedData(reinterpret_cast<char *>(plaintext), plaintext_len);

    // Clean up allocated memory
    delete[] plaintext;

    return decryptedData;
}

std::string encryptData(const std::string &data, const std::string &password)
{
    // Generate a 32-byte key from the password using SHA256
    unsigned char key[32];
    SHA256(reinterpret_cast<const unsigned char *>(password.c_str()), password.length(), key);

    // Use the first 16 bytes of the key as the IV (for testing purposes)
    std::string iv(reinterpret_cast<char *>(key), 16);

    // Set up the OpenSSL encryption context
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_CIPHER_CTX_init(ctx);

    // Initialize the encryption operation with AES 256 CBC and the generated key and IV
    if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, reinterpret_cast<const unsigned char *>(iv.c_str())))
    {
        std::cerr << "Error initializing encryption\n";
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }

    int len;
    int ciphertext_len;
    unsigned char *ciphertext = new unsigned char[data.length() + EVP_CIPHER_block_size(EVP_aes_256_cbc())];

    // Perform the encryption
    if (!EVP_EncryptUpdate(ctx, ciphertext, &len, reinterpret_cast<const unsigned char *>(data.c_str()), data.length()))
    {
        std::cerr << "Error during encryption\n";
        EVP_CIPHER_CTX_free(ctx);
        delete[] ciphertext;
        return "";
    }
    ciphertext_len = len;

    // Finalize the encryption
    if (EVP_EncryptFinal_ex(ctx, ciphertext + len, &len) <= 0)
    {
        std::cerr << "Error finalizing encryption\n";
        EVP_CIPHER_CTX_free(ctx);
        delete[] ciphertext;
        return "";
    }
    ciphertext_len += len;

    // Clean up the context
    EVP_CIPHER_CTX_free(ctx);

    // Convert the encrypted data to a string
    std::string encryptedData(reinterpret_cast<char *>(ciphertext), ciphertext_len);

    // Clean up allocated memory
    delete[] ciphertext;

    return encryptedData;
}

void connectToServerThread(const std::string &IP, int port, std::vector<std::string> &chatMessages, int &clientSocket, char *buffer)
{
    // Create client socket and connect
    try
    {
        clientSocket = createClientSocket(IP, port);

        // Once connected, handle the reception of messages
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead > 0)
        {
            buffer[bytesRead] = '\0';
            AddMessage(chatMessages, buffer); // Safely add received messages
        }
        else
        {
            AddMessage(chatMessages, "Error receiving message");
        }
    }
    catch (const std::exception &e)
    {
        AddMessage(chatMessages, e.what());
    }
}