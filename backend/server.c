#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif
#include <sys/stat.h>
#include "logic.c"

#define PORT 5000
#define BUFFER_SIZE 4096

// --- HELPER FUNCTIONS ---

const char* get_mime_type(const char* path) {
    const char* ext = strrchr(path, '.');
    if (!ext) return "text/plain";
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".json") == 0) return "application/json";
    return "text/plain";
}

void send_response(SOCKET client_socket, const char* header, const char* body) {
    send(client_socket, header, strlen(header), 0);
    send(client_socket, body, strlen(body), 0);
}

void send_json(SOCKET client_socket, const char* json_body) {
    char header[512];
    sprintf(header, 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %llu\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n", 
        (unsigned long long)strlen(json_body));
    send_response(client_socket, header, json_body);
}

void send_file(SOCKET client_socket, const char* filepath) {
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        if (strncmp(filepath, "./", 2) == 0) f = fopen(filepath + 2, "rb");
    }

    if (!f) {
        const char* msg = "HTTP/1.1 404 Not Found\r\n\r\nFile Not Found";
        send(client_socket, msg, strlen(msg), 0);
        return;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* content = (char*)malloc(fsize + 1);
    fread(content, 1, fsize, f);
    content[fsize] = 0;
    fclose(f);

    char header[512];
    sprintf(header, 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n\r\n", 
        get_mime_type(filepath), fsize);

    send(client_socket, header, strlen(header), 0);
    send(client_socket, content, fsize, 0);
    free(content);
}

// --- API HANDLERS ---

void handle_get_state(SOCKET client_socket) {
    update_system_state(); 

    // Allocate buffer (Increased for customer list just in case, though mainly for transactions)
    char* buffer = (char*)malloc(1024 * 1024); 
    strcpy(buffer, "{ \"transactions\": [");

    for (int i = 0; i < state.tx_count; i++) {
        Transaction* t = &state.all_transactions[i];
        char tx_json[512];
        sprintf(tx_json, 
            "{\"id\":%d, \"sender\":%d, \"receiver\":%d, \"amount\":%.2f, \"urgency\":%d, \"tier\":%d, \"status\":%d, "
            "\"base_priority\":%.2f, \"effective_priority\":%.2f, \"arrival\":%lld, \"unlock\":%lld}%s",
            t->id, t->sender_id, t->receiver_id, t->amount, t->urgency, t->tier, t->status,
            t->base_priority, t->effective_priority, (long long)t->arrival_time, (long long)t->unlock_time,
            (i < state.tx_count - 1) ? "," : "");
        strcat(buffer, tx_json);
    }
    
    // Add Customers (Securely? Admin sees names/ids, no pins)
    strcat(buffer, "], \"customers\": [");
    for (int i=0; i < state.customer_count; i++) {
        Customer* c = &state.all_customers[i];
         char cust_json[256];
         sprintf(cust_json, 
            "{\"id\":%d, \"name\":\"%s\", \"tier\":%d, \"balance\":%.2f}%s",
            c->account_number, c->name, c->tier, c->balance,
            (i < state.customer_count - 1) ? "," : "");
         strcat(buffer, cust_json);
    }

    strcat(buffer, "], \"stats\": {");
    char stats[256];
    sprintf(stats, "\"processed\":%d, \"cancelled\":%d, \"waiting_pq\":%d, \"locked\":%d } }", 
        state.processed_count, state.cancelled_count, state.priority_queue->size, state.time_lock_queue->size);
    strcat(buffer, stats);

    send_json(client_socket, buffer);
    free(buffer);
}

void handle_create_customer(SOCKET client_socket, char* body) {
    // {"name":"Ayush", "pin":1234, "tier":0, "balance":5000}
    char name[50] = "Unknown";
    int pin=0, tier=0;
    double balance=0;

    char* pName = strstr(body, "\"name\":\"");
    if(pName) {
        sscanf(pName + 8, "%[^\"]", name);
    }
    char* pPin = strstr(body, "\"pin\":");
    if(pPin) pin = atoi(pPin + 6);
    
    char* pTier = strstr(body, "\"tier\":");
    if(pTier) tier = atoi(pTier + 7);
    
    char* pBal = strstr(body, "\"balance\":");
    if(pBal) balance = atof(pBal + 10);

    Customer* c = create_customer(name, pin, tier, balance);
    if(c) {
        char resp[128];
        sprintf(resp, "{\"status\":\"ok\", \"account_number\":%d}", c->account_number);
        send_json(client_socket, resp);
    } else {
        send_json(client_socket, "{\"error\":\"Limit Reached\"}");
    }
}

void handle_login(SOCKET client_socket, char* body) {
    // {"id":50000, "pin":1234}
    int id=0, pin=0;
    char* pId = strstr(body, "\"id\":");
    if(pId) id = atoi(pId + 5);
    char* pPin = strstr(body, "\"pin\":");
    if(pPin) pin = atoi(pPin + 6);

    Customer* c = find_customer(id);
    if(c && c->pin == pin) {
        char resp[256];
        sprintf(resp, "{\"status\":\"ok\", \"name\":\"%s\", \"tier\":%d, \"balance\":%.2f}", c->name, c->tier, c->balance);
        send_json(client_socket, resp);
    } else {
        send_json(client_socket, "{\"error\":\"Invalid Credentials\"}");
    }
}

void handle_api_request(SOCKET client_socket, char* method, char* path, char* body) {
    if (strcmp(path, "/api/state") == 0 && strcmp(method, "GET") == 0) {
        handle_get_state(client_socket);
    } 
    else if (strcmp(path, "/api/customer") == 0 && strcmp(method, "POST") == 0) {
        handle_create_customer(client_socket, body);
    }
    else if (strcmp(path, "/api/login") == 0 && strcmp(method, "POST") == 0) {
        handle_login(client_socket, body);
    }
    else if (strcmp(path, "/api/transaction") == 0 && strcmp(method, "POST") == 0) {
        // {"sender":50000, "receiver":50001, "pin":1234, "amount":100, "urgency":0}
        double amount = 0;
        int sender=0, receiver=0, pin=0, urgency=0;
        
        char* pS = strstr(body, "\"sender\":"); if(pS) sender = atoi(pS+9);
        char* pR = strstr(body, "\"receiver\":"); if(pR) receiver = atoi(pR+11);
        char* pP = strstr(body, "\"pin\":"); if(pP) pin = atoi(pP+6);
        char* pA = strstr(body, "\"amount\":"); if(pA) amount = atof(pA+9);
        char* pU = strstr(body, "\"urgency\":"); if(pU) urgency = atoi(pU+10);

        int res = create_transaction(sender, receiver, pin, amount, urgency);
        
        if (res == 0) send_json(client_socket, "{\"status\":\"ok\"}");
        else if (res == 1) send_json(client_socket, "{\"error\":\"Invalid Sender\"}");
        else if (res == 2) send_json(client_socket, "{\"error\":\"Receiver Not Found\"}");
        else if (res == 3) send_json(client_socket, "{\"error\":\"Wrong PIN\"}");
        else if (res == 4) send_json(client_socket, "{\"error\":\"Insufficient Funds\"}");
        else send_json(client_socket, "{\"error\":\"Unknown Error\"}");
    }
    else if (strcmp(path, "/api/process") == 0 && strcmp(method, "POST") == 0) {
        Transaction* t = process_next_transaction();
        if(t) send_json(client_socket, "{\"status\":\"processed\"}");
        else send_json(client_socket, "{\"status\":\"empty\"}");
    }
    else if (strcmp(path, "/api/cancel") == 0 && strcmp(method, "POST") == 0) {
        int id = 0;
        char* pId = strstr(body, "\"id\":");
        if(pId) id = atoi(pId + 5);
        cancel_transaction(id);
        send_json(client_socket, "{\"status\":\"cancelled\"}");
    }
    else if (strcmp(path, "/api/unlock") == 0 && strcmp(method, "POST") == 0) {
        int id = 0;
        char* pId = strstr(body, "\"id\":");
        if(pId) id = atoi(pId + 5);
        force_unlock(id);
        send_json(client_socket, "{\"status\":\"unlocked\"}");
    }
    else {
        send_json(client_socket, "{\"error\":\"Not Found\"}");
    }
}

int main() {
#ifdef _WIN32
    WSADATA wsa;
#endif
    SOCKET server, client;
    struct sockaddr_in server_addr, client_addr;
    int addr_len = sizeof(client_addr);

    init_state();

    // Init with Admin/Demo? No, user will create.

#ifdef _WIN32
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;
#endif
    if ((server = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) return 1;

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed\n");
        return 1;
    }

    listen(server, 3);
    printf("Server listening on port %d...\n", PORT);

    while (1) {
        client = accept(server, (struct sockaddr *)&client_addr, &addr_len);
        if (client == INVALID_SOCKET) continue;

        char buffer[BUFFER_SIZE] = {0};
        recv(client, buffer, BUFFER_SIZE, 0);

        char method[16], path[256];
        sscanf(buffer, "%s %s", method, path);
        
        char* body = strstr(buffer, "\r\n\r\n");
        if (body) body += 4;
        else body = "";

        // Route
        if (strncmp(path, "/api/", 5) == 0) {
            handle_api_request(client, method, path, body);
        } else {
            // Serve static files
            char filepath[512] = "./frontend";
            if (strcmp(path, "/") == 0) strcat(filepath, "/index.html");
            else strcat(filepath, path);
            send_file(client, filepath);
        }
        closesocket(client);
    }

    closesocket(server);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
