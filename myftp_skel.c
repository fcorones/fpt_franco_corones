#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <err.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 512

#define MSG_220 "220 srvFtp version 1.0\r\n"

/**
 * function: receive and analize the answer from the server
 * sd: socket descriptor
 * code: three leter numerical code to check if received
 * text: normally NULL but if a pointer if received as parameter
 *       then a copy of the optional message from the response
 *       is copied
 * return: result of code checking
 **/
bool recv_msg(int sd, int code, char *text)
{
    char buffer[BUFSIZE], message[BUFSIZE];
    int recv_s, recv_code;

    // receive the answer

    recv_s = read(sd, buffer, BUFSIZE); //////

    // error checking
    if (recv_s < 0)
        warn("error receiving data");
    if (recv_s == 0)
        errx(1, "connection closed by host");

    // parsing the code and message receive from the answer
    sscanf(buffer, "%d %[^\r\n]\r\n", &recv_code, message);
    printf("%d %s\n", recv_code, message);
    // optional copy of parameters
    if (text)
        strcpy(text, message);
    // boolean test for the code
    return (code == recv_code) ? true : false;
}

/**
 * function: send command formated to the server
 * sd: socket descriptor
 * operation: four letters command
 * param: command parameters
 **/
void send_msg(int sd, char *operation, char *param)
{
    char buffer[BUFSIZE] = "";

    // command formating
    if (param != NULL)
        sprintf(buffer, "%s %s\r\n", operation, param);
    else
        sprintf(buffer, "%s\r\n", operation);

    // send command and check for errors

    //"OPCION" y "PARAMETRO" se formatean arriba y se envian por buffer

    if (send(sd,buffer,strlen(buffer),0) < 0)
        {
            perror("ERROR en send_msg");
            close(sd);
            exit(1);
        }
}

int port(int sd) {
   
    // crear un nuevo socket
    int data_sd = socket(AF_INET, SOCK_STREAM, 0);

    // verificar 
    if (data_sd < 0) {
        perror("ERROR: failed to create data socket");
        exit(EXIT_FAILURE);
    }

    // configuración 
    struct sockaddr_in addr2;
    socklen_t addr_len = sizeof(addr2);

    addr2.sin_family = AF_INET; 
    addr2.sin_addr.s_addr = htonl(INADDR_ANY); // cualqueir ip
    addr2.sin_port = htons(0); 

    // bindear socket
    if (bind(data_sd, (struct sockaddr*)&addr2, sizeof(addr2)) < 0) {
        close(data_sd);
        perror("ERROR: falla en bind comando port");
        exit(1);
    }

    // obtener puerto
    if (getsockname(data_sd, (struct sockaddr*)&addr2, &addr_len) < 0) {
        close(data_sd);
        perror("ERROR: error en getsockname en port");
        exit(1);
    }

    // esto lo q hace es extraer la ip y puerto del socket
    unsigned char* ip = (unsigned char*)&addr2.sin_addr.s_addr;
    unsigned char* port = (unsigned char*)&addr2.sin_port;

    // parametro a seguir
    char port_param[BUFSIZE];
    sprintf(port_param, "%d,%d,%d,%d,%d,%d",
            ip[0], ip[1], ip[2], ip[3], ntohs(addr2.sin_port) / 256, ntohs(addr2.sin_port) % 256);

    //enviamos comando port al servidor
    send_msg(sd, "PORT", port_param);

    // escuchamos....
    if (listen(data_sd, 1) < 0) {
        perror("ERROR: failed to listen on data socket");
        close(data_sd);
        exit(EXIT_FAILURE);
    }
    
    // si todo sale bien retornamos
    return data_sd;
}


/**
 * function: simple input from keyboard
 * return: input without ENTER key
 **/
char *read_input()
{
    char *input = malloc(BUFSIZE);
    if (fgets(input, BUFSIZE, stdin))
    {
        return strtok(input, "\n");
    }
    return NULL;
}

/**
 * function: login process from the client side
 * sd: socket descriptor
 **/
void authenticate(int sd)
{
    char *user, desc[100];
    char *pass;
    int code;

    // ask for user
    printf("username: ");
    user = read_input();

    // send the command to the server

    send_msg(sd,"USER",user);


    // // relese memory
     //free(input);
    // memset(input,0,strlen(input));

    // // wait to receive password requirement and check for errors
    //ahora debriamos escuchar para que me solicite la contraseña

    if (!recv_msg(sd, 331, desc)) {
        printf("Failed to receive password from server.\n");
        return;
    }

    // // ask for password
    //free(input);
    
     printf("passwd: ");
     pass = read_input();

    // // send the command to the server

    send_msg(sd,"PASS",pass);

    // // release memory
    // free(input);
    // memset(desc,0,sizeof(desc));
    // memset(input,0,strlen(input));

    // // wait for answer and process it and check for errors
    if (!recv_msg(sd, 230, desc)) {
        printf("Auth failed. User could not log in.\n");
        exit(1);
    }
}

/**
 * function: operation get
 * sd: socket descriptor
 * file_name: file name to get from the server
 **/
void get(int sd, char *file_name) {

    char desc[BUFSIZE], buffer[BUFSIZE];
    int f_size, recv_s, r_size = BUFSIZE;
    FILE *file;

    // send the PORT command to the server
    int data_socket = port(sd);

    // send the RETR command to the server
    send_msg(sd, "RETR", file_name);

    // check for the response
     if (!recv_msg(sd, 299, desc)) {
        printf("Error: no se pudo iniciar la transferencia del archivo.\n");
        return;
    }
    // parsing the file size from the answer received
    // "File %s size %ld bytes"
    sscanf(buffer, "File %*s size %d bytes", &f_size);

    // open the file to write
    file = fopen(file_name, "w");
     if (file == NULL) {
        perror("ERROR: failed to open file");
        return;
    }
    //accepting connection from the server

    struct sockaddr_in data_addr;
    socklen_t data_addr_len = sizeof(data_addr);

    int data_sd = accept(data_socket, (struct sockaddr*)&data_addr, &data_addr_len);

    if (data_sd < 0) {
        perror("ERROR: failed to accept data connection");
        fclose(file);
        return;
    }
    //receive the file
    while ((recv_s = recv(data_sd, buffer, r_size, 0)) > 0) {
        fwrite(buffer, sizeof(char), recv_s, file);
    }

    // close the file
    fclose(file);

    //close data socket
    close(data_sd);

    // receive the OK from the server
    recv_msg(sd, 226, NULL);
}



/**
 * function: operation quit
 * sd: socket descriptor
 **/
void quit(int sd)
{
    // send command QUIT to the client

    send_msg(sd,"QUIT",NULL);
    // receive the answer from the server
    recv_msg(sd,221,NULL);
}

/**
 * function: make all operations (get|quit)
 * sd: socket descriptor
 **/
void operate(int sd)
{
    char *input, *op, *param;

    while (true)
    {
        printf("Operation: ");
        input = read_input();
        if (input == NULL)
            continue; // avoid empty input
        op = strtok(input, " ");
        // free(input);
        if (strcmp(op, "get") == 0)
        {
            param = strtok(NULL, " ");
            get(sd, param);
        }
        else if (strcmp(op, "quit") == 0)
        {
            quit(sd);
            break;
        }
        else
        {
            // new operations in the future
            printf("TODO: unexpected command\n");
        }
        free(input);
    }
    free(input);
}

/**
 * Run with
 *         ./myftp <SERVER_IP> <SERVER_PORT>
 **/
int main(int argc, char *argv[])
{
    int sd;
    struct sockaddr_in addr;
    char buffer_enviado[1024];
    char buffer_recibido[1024];

    // arguments checking

    if (argc != 3)
    {
        printf("\nUso: %s <IP> <Puerto>\n", argv[0]);
        exit(1);
    }

    // create socket and check for errors

    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0)
    {
        close(sd);
        perror("Falla creacion socket");
        exit(1);
    }
    // set socket data

    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &addr.sin_addr) <= 0)
    {
        close(sd);
        perror("IP inválida");
        exit(1);
    }

    // connect and check for errors

    printf("\n--[C] Intentando conectar a %s %s--\n", argv[1], argv[2]);

    if (connect(sd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(sd);
        perror("Falla connect");
        exit(1);
    }
    else
    {
        printf("\n[C] Conexión exitosa\n");
    }


    // if receive hello proceed with authenticate and operate if not warning
    //RECIBE MENSAJE DE BIENVENIDA
        if(!recv_msg(sd, 220, NULL)){
        warn("ERROR: hello message was not received");
    }else{
       authenticate(sd);
       operate(sd);
    }
    
    // close socket
    close(sd);

    printf("\nSE CERRARÁ EL CLIENTE\n");
    return 0;
}





    // if (strcmp(buffer_recibido,MSG_220)==0) // si realmente recibimos el HELLO, continuar
    // {

    // } else  // si no se recibio hello o se recibio otra cosa, chau chau
    // {
    //     printf("\n[C] - No se recibió HELLO, cerrando conexión");
    //     close(sd);
    //     exit(1);
    // }