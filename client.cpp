/**
* Projekt cislo 1 do predmetu IPK
* Nazev: Aplikace klient/server pro prenos souboru
* Autor: Ermak Aleksei, xermak00@stud.fit.vutbr.cz
* Rok: 2017
*/

/* Potrebne knihovny */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fstream>
#include <fcntl.h>
#include <time.h>

#define MAX_PORT 65535
using namespace std;

/* Navratove kody programu */
enum err_codes {
    CODE_OK = 0,
    CODE_ARG,
    CODE_CONNECT,
    CODE_COMMUNICATE,
    CODE_FILE,
    CODE_REQUEST,
    CODE_RESPONSE,
    CODE_SEND,
    CODE_RECV
};

/* Prikazy pro aplikace klienta */
enum commands {
    PUT,    /* zkopirovat soubor z LOCAL-PATH do adresare REMOTEPATH */
    GET,    /* zkopirovat soubor z REMOTE-PATH do aktualniho lokalniho adresare ci na misto urcene pomoci LOCAL-PATH je-li uvedeno */
    LST,    /* vypise obsah vzdaleneho adresare na standardni vystup */
    MKD,    /* vytvori adresar specifikovany v REMOTE-PATH na serveru */
    RMD,    /* odstrani adresar specifikovany V REMOTE-PATH ze serveru */
    DEL,    /* smaze soubor urceny REMOTE-PATH na serveru */
    ERR     /* chyba */
};

/* Struktura prikazu uzivatele */
struct tRequest {
    int command;
    int port;
    string host_name;
    string rem_path;
    string loc_path;
};

/* Funkce ziskava argumenty prikazove radky, se kterymi byl spusten program */
int get_args(tRequest *req, int argc, char *argv[]) {
    tRequest request = {ERR, 0, "", "", ""};
    /* chybny pocet argumentu -> chyba */
    if (argc < 3 || argc > 4)
        return CODE_ARG;
    /* prvnim argumentem musi byt prikaz */
    string command = argv[1];
    /* druhym argumentem musi byt vzdalena cesta na serveru s jmenem hostu a portu */
    char *del = strtok(argv[2], "//");
    /* http: */
    if (strcmp(del, "http:")) return CODE_ARG;
    /* hostname */
    del = strtok(NULL, "/:");
    request.host_name = del;
    /* port */
    del = strtok(NULL, ":");
    request.port = strtol(del, &del, 10);
    if (request.port > MAX_PORT) return CODE_ARG;
    /* vzdalena cesta */
    request.rem_path = del;
    /* nastavime prikaz */
    if (!command.compare("put")) request.command = PUT;
    else if (!command.compare("mkd")) request.command = MKD;
    else if (!command.compare("get")) request.command = GET;
    else if (!command.compare("lst")) request.command = LST;
    else if (!command.compare("del")) request.command = DEL;
    else if (!command.compare("rmd")) request.command = RMD;
    else return CODE_ARG;
    /* pridame ke vzdalenym cestam typ */
    switch (request.command) {
        case MKD: case LST: case RMD: request.rem_path.append("?type=folder"); break;
        case PUT: case GET: case DEL: request.rem_path.append("?type=file"); break;
        default : return CODE_ARG;
    }
    /* treti argument (lokalni cesta na pocitace uzivatele) je povinny pro prikaz put */
    if (request.command == PUT && argc != 4)
        return CODE_ARG;
    /* pokud zadan treti argument, tak ten musi byt lokalni cestou na pocitace klienta */
    if (argc == 4) request.loc_path = argv[3];
    /* treti argument urcen jen pro prikazy PUT a GET */
    else if (argc > 3 && request.command != PUT && request.command != GET)
        return CODE_ARG;
    else request.loc_path = "./";
    *req = request;
    return CODE_OK;
}

/* Funkce se pripoje ke vzdalenemu serveru pres novy socket */
int connect_to_server(int *tcp_sock, tRequest request) {
    struct hostent *server_name;
    struct sockaddr_in server_addr;
    /* vytvorime novy socket */
    *tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*tcp_sock < 0)
        return CODE_CONNECT;
    /* ziskame jmeno hostu */
	if ((server_name = gethostbyname(request.host_name.c_str())) == NULL)
		return CODE_CONNECT;
    /* nastavime adresu portu serveru */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(request.port);
    /* prekopirujeme jmeno hostu do adresy serveru */
    memcpy(&server_addr.sin_addr, server_name->h_addr, server_name->h_length);
    /* pripojime se */
    if (connect(*tcp_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
        return CODE_CONNECT;
    return CODE_OK;
}

string get_comm(int comm) {
    switch (comm) {
        case PUT: case MKD: return "PUT";
        case GET: case LST: return "GET";
        case RMD: case DEL: return "DELETE";
        default: return NULL;
    }
}

/* Funkce ziskava priponu a velikost pozadovaneho souboru */
int file_info_not_receive(string path, string *mime_type, int *length) {
    char buffer[BUFSIZ];
    int ret, fin = open(path.c_str(), O_RDONLY);
    if (fin < 0) return CODE_FILE;
    /* velikost */
    while(1) {
        ret = read(fin, buffer, BUFSIZ);
        if (ret < 0) return CODE_FILE;
        if (ret == 0) break;
        *length += ret;
    }
    close(fin);
    path = "file --mime-type -b " + path;
    string m_type("");
    FILE* type = popen(path.c_str(), "r");
    while (fgets(buffer, BUFSIZ, type) != NULL) m_type = m_type + buffer;
    m_type.pop_back();
    *mime_type = m_type;
    pclose(type);
    return CODE_OK;
}

/* Funkce vytvori HTTP pozadavek a odesle ho serveru */
int send_request_call_error(tRequest request, int socket) {
    string req_text = get_comm(request.command) + " " + request.rem_path + " HTTP/1.1\r\n";
    time_t t = time(NULL);
    string t_st = (string)ctime(&t);
    t_st.erase(t_st.end()-1);
    req_text += "Date: " + t_st + " GMT\r\n";
    req_text += "Accept: identity\r\n";
    req_text += "Accept-Encoding: identity";
    if (request.command == PUT) {
        req_text += "\r\n";
        /* ziskame informace o souboru */
        string mime_type;
        int length = 0;
        if (file_info_not_receive(request.loc_path, &mime_type, &length))
            return CODE_FILE;
        /* pridame informace do hlavicky */
        req_text += "Content-Type: " + mime_type + "\r\n";
        req_text += "Content-Length: " + to_string(length);
    }
    req_text += "\r\n\r\n";
    /* odesleme pozadavek */
    if ((send(socket, req_text.c_str(), req_text.length(), 0)) < 0)
        return CODE_REQUEST;

    return CODE_OK;
}

/* Funkce prijima odpoved od servera */
int response_with_error(int socket) {
    char buffer[BUFSIZ];
    int ret;
    string message;
    size_t end_msg;
    /* prijmeme a zpracujeme odpoved */
    while ((ret = read(socket, buffer, 1)) > 0) {
        message.append(buffer, ret);
        if ((end_msg = message.find("\r\n\r\n")) != string::npos) {
            message.erase(end_msg);
            break;
        }
        bzero(buffer, BUFSIZ);
    }
    if (message.find("HTTP/1.1 200 OK\r\n")) {
        ret = message.find_last_of("\r\n");
        cerr << message.substr(ret+1, message.length()) + "\n";
        return CODE_RESPONSE;
    }
    return CODE_OK;
}

/* Funkce odesila pozadovany soubor serveru, prikaz PUT */
int send_file(tRequest req, int socket) {
    char buffer[BUFSIZ];
    int fin, ret;
    /* odesilame soubor */
    fin = open(req.loc_path.c_str(), O_RDONLY);
    if (fin < 0) return CODE_FILE;
    while (1) {
        ret = read(fin, buffer, BUFSIZ);
        if (ret == 0) break;
        if (ret < 0) return CODE_FILE;
        if (write(socket, buffer, ret) < 0) return CODE_SEND;
    }
    close(fin);
    /* prijmeme a zpracujeme odpoved od serveru */
    if (response_with_error(socket))
        return  CODE_RESPONSE;
    return CODE_OK;
}

/* Funkce nahraje soubor, prikaz GET */
int recv_file(tRequest req, int socket) {
    int ret;
    char buffer[BUFSIZ];
    bzero(buffer, BUFSIZ);
    /* prijmeme a zpracujeme odpoved od serveru */
    if (response_with_error(socket))
        return  CODE_RESPONSE;
    /* pokud uzivatel nezadal lokalni cestu, udelame novy soubor */
    /* nazev je stejny jako u souboru na serveru */
    if (req.loc_path.find_last_of("/") == (req.loc_path.length()-1)) {
        req.rem_path = req.rem_path.substr(0, req.rem_path.find("?", 0));  /* ?type=[file/folder] */
        req.loc_path.append(req.rem_path.substr((req.rem_path.find_last_of("/")) + 1));
    }
    int fout = open(req.loc_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    /* nahravame soubor */
    if (fout == -1)
        return CODE_FILE;
    while(1) {
        ret = read(socket, buffer, BUFSIZ);
        if (ret < -1) return CODE_FILE;
        if (ret == 0) break;
        if (write(fout, buffer, ret) < -1) return CODE_RECV;
    }
    close(fout);
    return CODE_OK;
}

/* Funkce prijima obsah vzdaleneho adresare a vypisuje ho stdout */
int recv_stat(int socket) {
    /* prijmeme a zpracujeme odpoved od serveru */
    if (response_with_error(socket))
        return  CODE_RESPONSE;
    int ret;
    char buffer[BUFSIZ];
    string ls;
    while(1) {
        ret = read(socket, buffer, BUFSIZ);
        if (ret < -1) return CODE_FILE;
        if (ret == 0) break;
        ls.append(buffer);
    }
    cout << ls + "\n";
    return CODE_OK;
}

/* Funkce pro komunikace se serverem */
int communicate_with_server(tRequest request, int *sock) {
    int socket = *sock;
    /* vytvorime a odesleme pozadavek */
    if (send_request_call_error(request, *sock))
        return CODE_COMMUNICATE;
    /* vyplnime pozadavek klienta */
    int how_done;
    switch (request.command) {
        case PUT: how_done = send_file(request, socket); break;
        case GET: how_done = recv_file(request, socket); break;
        case LST: how_done = recv_stat(socket); break;
        case DEL:   /* od techto operaci ocekavame jenom */
        case RMD:   /* odpoved od vzdaleneho servera */
        case MKD: how_done = response_with_error(socket); break;
    }
    close(socket);
    return how_done;
}

/* Hlavni funkce */
int main(int argc, char *argv[]) {
    int socket;
    tRequest request;
    /* inicializujeme a ziskame prikaz od klienta */
    int arg_not_succ = get_args(&request, argc, argv);
    /* zkontrolujeme prikaz */
    if (arg_not_succ) {
        cerr << "Error: Bad arguments\n";
        return EXIT_FAILURE;
    }
    /* inicializujeme socket a pripojime se k serveru */
    int connect_not_succ = connect_to_server(&socket, request);
    /* zkontrolujeme pripojeni */
    if (connect_not_succ) {
        cerr << "Error: Connect\n";
        return EXIT_FAILURE;
    }
    /* komunikujeme se serverem */
    int communication_state = communicate_with_server(request, &socket);
    return communication_state;
}
