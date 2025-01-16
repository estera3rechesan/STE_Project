#include <stdio.h>     
#include <stdlib.h>  
#include <string.h> 
#include <unistd.h> 
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h> 
#include <fcntl.h> 

#define PORT 5000          // portul pe care serverul va asculta pentru conexiuni
#define BUFFER_SIZE 1024   // dimensiunea buffer-ului pentru citirea datelor
#define ERROR_PAGE "404.html" 

typedef int socklen_t; //folosit pentru lungimea adresei

void handle_client(int client_sock) {
    char buffer[BUFFER_SIZE];    //buffer pt citire cerere
    char method[16], path[256], protocol[16];  //metoda HTTP, calea și versiunea HTTP
    char response[BUFFER_SIZE];  //buffer cu raspunsul pt client
    int bytes_read; 

    //CITIRE CERERE
    memset(buffer, 0, BUFFER_SIZE);  // reset buffer
    /*recv = primeste date de la client
    client_sock = de unde se citesc datele
    buffer = unde vor fi stocate datele
    BUFFER_SIZE - 1 = dimensiunea maximă a datelor ce pot fi citite + spatiu pt \
    0 = setari suplimentare (nu exista)
    */
    bytes_read = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read <= 0) {   //--> nu s-a citit nimic/eroare
        close(client_sock);  //--> inchidem conexiunea cu clientul
        return;
    }

    //analizeaza sirul buffer (care contine cererea HTTP primita de la client) 
    //si extrage componentele esentiale ale acesteia
    sscanf(buffer, "%15s %255s %15s", method, path, protocol);
    //%15s: un sir de maximum 15 caractere -> stocat in method ...

    //calea index.html este folosita direct pentru a deschide fisierul --> nu ne trebe /
    if (path[0] == '/') {
        memmove(path, path + 1, strlen(path));  
        //Mutarea copiaza fiecare caracter, suprascriindu-l pe cel din poziția anterioara
    }

    if (strlen(path) == 0) { //dacă nu s-a specificat o cale în cererea HTTP
        strcpy(path, "index.html");  //serverul presupune ca userul vrea sa acceseze pagina implicita (index.html)
    }

    //RASPUNS CERERE
    int file_fd = open(path, O_RDONLY);  //incearca sa deschida fisierul cu calea path in modul read-only
    if (file_fd < 0) { //daca nu exista
        file_fd = open(ERROR_PAGE, O_RDONLY); //--> deschidem pag de eroare
        if (file_fd < 0) {  //daca nici pag de eroare nu exista
            sprintf(response, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n");
            send(client_sock, response, strlen(response), 0);  //--> trimitem un raspuns de eroare 500
            close(client_sock);  //la final inchidem conexiunea cu clientul
            return;
        }
        //daca pag de eroare exista
        sprintf(response, "HTTP/1.1 404 Not Found\r\n"); //--> trimitem un raspuns de eroare 400
    }
    else {  //daca exista
        sprintf(response, "HTTP/1.1 200 OK\r\n");  //--> trimitem un raspuns 200 OK
    }

    strcat(response, "Content-Type: text/html\r\n\r\n");
    /*Header-ul Content-Type:
    Indică browserului client ce tip de conținut să se aștepte. 
    ex: text/html = raspunsul este o pagină web în format HTML.*/
    send(client_sock, response, strlen(response), 0);  //--> trimitem raspunsul complet clientului

    //Citeste pana la BUFFER_SIZE octeti din fisierul file_fd si ii stocheaza in buffer
    while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
        send(client_sock, buffer, bytes_read, 0);  //--> se trimite fisierul
    }

    close(file_fd);
    close(client_sock);
}

int main() {
    int server_sock, client_sock;  // socket - pentru server si client
    struct sockaddr_in server_addr, client_addr;  // structuri pentru adresele serverului si clientului
    socklen_t client_addr_len = sizeof(client_addr);  // lungimea adresei clientului

    //Socketul serverului
    /*AF_INET = utilizăm IPv4
    SOCK_STREAM = socket de tip flux, adică folosim protocolul TCP
    0 = sistemul alege automat protocolul corect pentru tipul de socket specificat
    */
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    server_addr.sin_family = AF_INET; //familia de adrese IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY; //accepta conexiuni de la orice adresa
    server_addr.sin_port = htons(PORT); //portul pe care serverul va asculta pentru conexiuni

    /*bind() este folosit pentru a asocia un socket
    (socketul, adresa la care vrem sa legam socketul, dimensiunea adresei)
    */
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        return 1;
    }

    //ASCULTARE CONEXIUNI --> listen(de la cine, nr max de conexiuni in asteptare)
    if (listen(server_sock, 5) < 0) {
        perror("Listen failed");
        close(server_sock);
        return 1;
    }

    printf("Serverul este activ pe portul %d\n", PORT);

    //ACCEPTARE CONEXIUNI
    while (1) {
        //accept() blochează programul până când un client se conectează la server
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_sock < 0) { //daca nu s-a putut accepta conexiunea
            continue;  //--> asteapta urmatoarea conexiune
        }
        printf("Client conectat: %s\n", inet_ntoa(client_addr.sin_addr)); //daca a functionat --> afiseaza adresa clientului
        handle_client(client_sock);  // --> procesam cererea
    }

    close(server_sock);
    return 0;
}
