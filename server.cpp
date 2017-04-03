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
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fstream>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>

#define MIN_PORT 1024
#define MAX_PORT 65535
using namespace std;

/* Navratove kody programu */
enum err_codes {
    CODE_OK = 0,
    CODE_ARG,
    CODE_CONNECT,
    CODE_COMMUNICATE,
    CODE_REQ,
    CODE_FILE,
    CODE_EXISTS,
    CODE_N_EMPTY,
    CODE_FILE_N_EXISTS,
    CODE_DIR_N_EXISTS,
    CODE_N_FILE,
    CODE_N_DIR,
    CODE_RESPONSE,
    CODE_SEND
};

/* Prikazy pro aplikace klienta */
enum commands {
    PUT,    /* zkopirovat soubor z LOCAL-PATH do adresare REMOTEPATH */
    GET,    /* zkopirovat soubor z REMOTE-PATH do aktualniho lokalniho adresare ci na misto urcene pomoci LOCAL-PATH je-li uvedeno */
    DEL,    /* smaze soubor urceny REMOTE-PATH na serveru */
    RMD,    /* odstrani adresar specifikovany V REMOTE-PATH ze serveru */
    LST,    /* vypise obsah vzdaleneho adresare na standardni vystup */
    MKD,    /* vytvori adresar specifikovany v REMOTE-PATH na serveru */
    ERR     /* chyba */
};

/* Struktura prikazu uzivatele */
struct tRequest {
    int command;
    int size;
    string type;
    string loc_path;
};

/* Funkce ziskava argumenty prikazove radky, se kterymi byl spusten program */
int get_args(string *root_folder, int *port, int argc, char *argv[]) {
    /* chybny pocet argumentu -> chyba */
    if (argc > 5 )
        return CODE_ARG;
    char *e_ptr;
    for (int i = 1; i < argc-1; i += 2) {
        /* je-li prvni argument "-r", druhy je korenovy adresar */
        if (!strcmp(argv[i], "-r")) {
            *root_folder = argv[i+1];
        }
        /* je-li prvni argument "-p", druhy je port */
        else if (!strcmp(argv[i], "-p")) {
            *port = strtol(argv[i+1], &e_ptr, 10);
            if (*e_ptr != '\0') return CODE_ARG;
        }
        else return CODE_ARG;
    }
    if (*port < MIN_PORT || *port > MAX_PORT) return CODE_ARG;
    return CODE_OK;
}

/* Funkce vytvori novy socket a zacne naslouchat prikazy */
int connect_to_network(struct sockaddr_in *serv, int *sock, int port) {
    /* vytvorime novy socket */
    int tcp_sock;
    struct sockaddr_in server;
    tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock < 0)
        return CODE_CONNECT;
    /* nastavime adresu portu serveru */
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr  = htonl(INADDR_ANY);
    /* nastavime parametry socketu, predtim smazeme predchozi nastaveni */
    int yes = 1;
    setsockopt(tcp_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    if (bind(tcp_sock, (struct sockaddr *)&server, sizeof(server)) < 0)
		return CODE_CONNECT;
    /* zacneme naslouchat prikazy */
	if (listen(tcp_sock, 5))
		return CODE_CONNECT;
    /* nastavime info o socketu a serveru */
    *sock = tcp_sock;
    *serv = server;
    return CODE_OK;
}

string code_name(int code, int head) {
    if (head) {
        switch (code) {
            case CODE_OK: return "200 OK";
            case CODE_N_FILE:
            case CODE_N_DIR:
            case CODE_REQ:
            case CODE_N_EMPTY:
            case CODE_EXISTS: return "400 Bad Request";
            case CODE_FILE_N_EXISTS:
            case CODE_DIR_N_EXISTS: return "404 Not Found";
            default: return NULL;
        }
    }
    else {
        switch (code) {
            case CODE_EXISTS: return "Already exists.";
            case CODE_N_FILE: return "Not a file.";
            case CODE_N_DIR: return "Not a directory.";
            case CODE_N_EMPTY: return "Directory not empty.";
            case CODE_FILE_N_EXISTS: return  "File not found.";
            case CODE_DIR_N_EXISTS: return "Directory not found.";
            default : return "Unknown error.";
        }
    }
}

/* Funkce vytvori a odesle odpoved */
int send_response(tRequest request, int socket, int code) {
    string response;
    response = "HTTP/1.1 " + code_name(code, true) + "\r\n";
    time_t t = time(NULL);
    string t_st = (string)ctime(&t);
    t_st.erase(t_st.end()-1);
    if (request.type.length() == 0)
        request.type = "text/plain";
    response += "Date: " + t_st + " GMT\r\n";
    response += "Content-Type: "+ request.type + "\r\n";
    response += "Content-Length: " + to_string(request.size) + "\r\n";
    response += "Content-Encoding: identity";
    if (code) {
        response += "\r\n\n";
        response += code_name(code, false) + "\r\n\r\n";
    }
    else response += "\r\n\r\n";
    /* odesleme odpoved */
    if (send(socket, response.c_str(), response.length(), 0) < 0) {
        return CODE_RESPONSE;
    }
    return CODE_OK;
}

/* Funkce zjisti z pozadavku klienta co vlastne chce */
tRequest find_out_req(string msg) {
    tRequest request;
    /* prikaz */
    string command = msg.substr(0, msg.find_first_of(" "));
    msg.erase(msg.begin(), msg.begin()+command.length() + 1);
    /* cesta na serveru */
    int point = msg.find_first_of(" ");
    request.loc_path = msg.substr(0, point);
    /* urcujeme co uzivatel potrebuje */
    point = request.loc_path.find_last_of("=");
    string type = request.loc_path.substr(point+1, request.loc_path.length());
    int folder = strcmp("folder", type.c_str());
    if (!command.compare("PUT"))
        request.command = folder ? PUT : MKD;
    else if (!command.compare("GET"))
        request.command = folder ? GET : LST;
    else if (!command.compare("DELETE"))
        request.command = folder ? DEL : RMD;
    else request.command = ERR;
    /* smazeme ?type=[folder/file] v ceste */
    request.loc_path = request.loc_path.substr(0, request.loc_path.find("?", 0));
    if (request.command == PUT) {
        point = msg.find("Content-Type: ");
        msg.erase(0, point+14);
        point = msg.find("\r\n");
        request.type = msg.substr(0, point);
        point = msg.find("Content-Length: ");
        msg.erase(0, point+16);
        request.size = strtol(msg.c_str(), NULL, 10);
    }
    else request.size = 0;
    return request;
}

/* Funkce ziskava priponu a velikost pozadovaneho souboru */
int file_info_not_receive(string path, string *mime_type, int *length) {
    int ret, fin = open(path.c_str(), O_RDONLY);
    char buffer[BUFSIZ];
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

/* Funkce urcuje je-li zadana cesta je adresarem */
bool is_directory(string path) {
    struct stat s;
    if(stat(path.c_str(), &s) == 0) {
        if( s.st_mode & S_IFDIR )
            return true;
        else return false;
    }
    return false;
}

/* Funkce urcuje je-li zadana cesta je souborem */
bool is_file(string path) {
    struct stat s;
    if (stat(path.c_str(), &s) == 0) {
        if( s.st_mode & S_IFREG )
            return true;
        else return false;
    }
    return false;
}

/* Funkce nahraje soubor, prikaz PUT */
int recv_file(tRequest req, int socket) {
    /* zkontrolujeme jestli soubor jiz existuje na serveru */
    if (is_file(req.loc_path)) {
        send_response(req, socket, CODE_EXISTS);
        return CODE_EXISTS;
    }
    char buffer[BUFSIZ];
    int ret, code = 0, total = 0;
    int fout = open(req.loc_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fout == -1) code = CODE_FILE;
    while(1) {
        if (total >= req.size) break;
        ret = read(socket, buffer, BUFSIZ);
        if (ret < -1) {
            code = CODE_FILE;
            break;
        }
        if (write(fout, buffer, ret) < -1) {
            code = CODE_FILE;
            break;
        }
        total += ret;
        bzero(buffer, BUFSIZ);
    }
    close(fout);
    req.size = total;
    /* odesleme odpoved */
    send_response(req, socket, code);
    return CODE_OK;
}

/* Funkce odesila soubor, prikaz GET */
int send_file(tRequest req, int socket) {
    /* zlontrolujeme nezadali li kient o soubor, ktery je na serveru adresarem */
    if (is_directory(req.loc_path)) {
        send_response(req, socket, CODE_N_FILE);
        return CODE_N_FILE;
    }
    /* zkontrolujeme jestli soubor existuje */
    if (!is_file(req.loc_path)) {
        send_response(req, socket, CODE_FILE_N_EXISTS);
        return CODE_FILE_N_EXISTS;
    }
    /* ziskame informace o souboru */
    string mime_type;
    char buffer[BUFSIZ];
    int fin, ret, length = 0;
    if (file_info_not_receive(req.loc_path, &mime_type, &length)) {
        send_response(req, socket, CODE_FILE);
        return CODE_FILE;
    }
    fin = open(req.loc_path.c_str(), O_RDONLY);
    if (fin < 0) {
        send_response(req, socket, CODE_FILE);
        return CODE_FILE;
    }
    /* pridame informace do hlavicky */
    req.size = length;
    req.type = mime_type;
    /* nejdrive odesleme odpoved */
    if (send_response(req, socket, CODE_OK))
        return CODE_RESPONSE;
    /* pak odesilame soubor */
    while (1) {
        ret = read(fin, buffer, BUFSIZ);
        if (ret == 0) break;
        if (ret < 0) return CODE_FILE;
        if (write(socket, buffer, ret) < 0) return CODE_SEND;
    }
    close(fin);
    return CODE_OK;
}

/* Funkce smaze pozadovany soubor, prikaz DEL */
int delt_file(tRequest req, int socket) {
    /* zkontrolujeme nezadali li kient o smazani souboru, ktery je na serveru adresarem */
    if (is_directory(req.loc_path)) {
        send_response(req, socket, CODE_N_FILE);
        return CODE_N_FILE;
    }
    /* zkontrolujeme jestli soubor existuje */
    if (!is_file(req.loc_path)) {
        send_response(req, socket, CODE_FILE_N_EXISTS);
        return CODE_FILE_N_EXISTS;
    }
    int code = 0;
    if (remove(req.loc_path.c_str()))
        code = CODE_FILE;
    send_response(req, socket, code);
    return CODE_OK;
}

/* Funkce smaze pozadovany adresar, prikaz RMD */
int delt_fold(tRequest req, int socket) {
    /* zkontrolujeme nezadali li kient o smazani adresare, ktery je na serveru souborem */
    if (is_file(req.loc_path)) {
        send_response(req, socket, CODE_N_DIR);
        return CODE_N_DIR;
    }
    /* zkontrolujeme jestli adresar existuje */
    if (!is_directory(req.loc_path)) {
        send_response(req, socket, CODE_DIR_N_EXISTS);
        return CODE_DIR_N_EXISTS;
    }
    int code = 0;
    if (rmdir(req.loc_path.c_str()))
        code = CODE_N_EMPTY;
    send_response(req, socket, code);
    return CODE_OK;
}

/* Funkce vytvori pozadovany adresar, prikaz MKD */
int make_dirc(tRequest req, int socket) {
    /* zkontrolujeme nezadali li kient o vytvoreni adresare, ktery uz je na serveru */
    if (is_directory(req.loc_path)) {
        send_response(req, socket, CODE_EXISTS);
        return CODE_EXISTS;
    }
    int code = 0;
    if (mkdir(req.loc_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
        code = CODE_FILE;
    if (send_response(req, socket, code))
        return CODE_FILE;
    return CODE_OK;
}

/* Funkce odesila obsah vzdaleneho adresare, prikaz LST */
int send_stat(tRequest req, int socket) {
    /* zkontrolujeme jestli adresar existuje */
    if (!is_directory(req.loc_path)) {
        /* zkontrolujeme nezadali li kient o vypis adresare, ktery je na serveru souborem */
        if (is_file(req.loc_path)) {
            send_response(req, socket, CODE_N_DIR);
            return CODE_N_DIR;
        }
        send_response(req, socket, CODE_DIR_N_EXISTS);
        return CODE_DIR_N_EXISTS;
    }
    /* nejdrive odesleme odpoved */
    if (send_response(req, socket, CODE_OK))
        return CODE_RESPONSE;
    FILE *ls;
    char buffer[BUFSIZ];
    char *ret = NULL;
    if ((ls = popen(("ls -C --color=never " + req.loc_path).c_str(), "r"))) {
        while (1) {
            ret = fgets(buffer, BUFSIZ, ls);
            if (ret == NULL) break;
            if (write(socket, buffer, strlen(ret)) < 0) return CODE_SEND;
        }
    }
    else return CODE_FILE;
    return CODE_OK;
}

/* Funkce pro komunikace s klienty */
int communicate_with_clients(int socket, struct sockaddr_in server, string root_folder) {
    int client_sock, pid = 0;
    socklen_t serv_size = sizeof(server);
    while(1) {
        /* inicializujeme novy socket pro spojeni */
        client_sock = accept(socket, (struct sockaddr *)&server, &serv_size);
        if (client_sock < 0) {
			return CODE_COMMUNICATE;
		}
        /* vytvarime nove procesy */
        pid = fork();
        /* pro rodice uzavreme socket */
        if (pid > 0) {
            close(client_sock);
        }
        else if (pid == 0) {
            string message = "";
            int n_byte;
            size_t end_msg;
            char buffer[1];
            /* ziskavame prikaz od klienta */
            while ((n_byte = read(client_sock, buffer, 1)) > 0) {
                message.append(buffer, n_byte);
                if ((end_msg = message.find("\r\n\r\n")) != string::npos) {
                    message.erase(end_msg);
                    break;
                }
            }
            /* zpracujeme pozadavek */
            tRequest request = find_out_req(message);
            /* zkontrolujeme pozadavek */
            if (request.command == ERR) return CODE_REQ;
            /* nastavime cestu ke korenovemu adresari, se kterym server pracuje */
            request.loc_path = root_folder + request.loc_path;
            /* udelame co klient chce */
            int code;
            switch (request.command) {
                case PUT: code = recv_file(request, client_sock); break;
                case GET: code = send_file(request, client_sock); break;
                case DEL: code = delt_file(request, client_sock); break;
                case RMD: code = delt_fold(request, client_sock); break;
                case MKD: code = make_dirc(request, client_sock); break;
                case LST: code = send_stat(request, client_sock); break;
            }
            close(client_sock);
            close(socket);
            exit(code);
        }
    }
    return CODE_OK;
}

void signal_hunter(int signal) {
    cerr << "Server skoncil pracovat s signalem " << signal << "\n";
    exit(CODE_OK);
}

/* Hlavni funkce */
int main(int argc, char *argv[]) {
    signal(SIGTERM, signal_hunter);
	signal(SIGINT, signal_hunter);
	signal(SIGCHLD, SIG_IGN);

    int port = 6677;
    string root_folder = "./";
    int welcome_socket;
    struct sockaddr_in server;
    /* ziskame cislo portu a korenovy adresar z prikazove radky */
    int arg_not_succ = get_args(&root_folder, &port, argc, argv);
    /* zkontrolujeme data */
    if (arg_not_succ) {
        cerr << "Chyba v argumentech!\n";
        return EXIT_FAILURE;
    }
    /* inicializujeme socket a zacneme cist prikazy */
    int connect_not_succ = connect_to_network(&server, &welcome_socket, port);
    /* zkontrolujeme pripojeni */
    if (connect_not_succ) {
        cerr << "Nepodarilo se propojit!\n";
        return EXIT_FAILURE;
    }
    /* komunikujeme s klienty */
    int communication_state = communicate_with_clients(welcome_socket, server, root_folder);
    close(welcome_socket);
    return communication_state;
}
