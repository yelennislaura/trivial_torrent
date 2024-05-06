// Trivial Torrent


// TODO: some includes here


#include "file_io.h"
#include "logger.h"
//includes afegits
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/wait.h>



// TODO: hey!? what is this?

/*
* This is the magic number (already stored in network byte order).
*/
static const uint32_t MAGIC_NUMBER = 0xde1c3233; // = htonl(0x33321cde);
static const uint8_t MSG_REQUEST = 0;
static const uint8_t MSG_RESPONSE_OK = 1;
static const uint8_t MSG_RESPONSE_NA = 2;


enum { RAW_MESSAGE_SIZE = 13 };


/**
* Main function.
*/


int main(int argc, char** argv)
{
    set_log_level(LOG_DEBUG);
    log_printf(LOG_INFO, "Trivial Torrent (build %s %s) by %s", __DATE__, __TIME__, "J. DOE and J. DOE");


    // ==========================================================================
    // Parse command line
    // ==========================================================================
    // TODO: some magical lines of code here that call other functions and do various stuff.
    
    
    if(argv[1] == NULL)
    {
    perror("Ha de tenir una llargada major a 0");
    return 1;
    }
    if (!strcmp(argv[1], "-l"))
    {
        //SERVER
        log_printf(LOG_INFO, "######################################################");
        log_printf(LOG_INFO, "###################ENTRANT SERVIDOR###################");
        log_printf(LOG_INFO, "######################################################");
        
	// Carregam el torrent t_server a partir de l'arxiu .torrent, la ruta corresponent
        assert(argv[3]!=NULL);
        struct torrent_t t_server;
        int ct_server = create_torrent_from_metainfo_file(argv[3], &t_server, "torrent_samples/server/test_file_server");
        if (ct_server == -1)
        {
            perror("No s'ha pogut crear");
            if(destroy_torrent(&t_server) == -1)
              perror("No s'ha pogut destruir");
            return 1;
        }

	// Cream el socket i comprovam que es pugui crear
        int s_server = socket(AF_INET, SOCK_STREAM, 0);

        if (s_server == -1)
        {
            perror("No s'ha pogut crear el socket");
            if(destroy_torrent(&t_server) == -1)
              perror("No s'ha pogut destruir");
            return 1;
        }

	// Cream l'adreça del servidor
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(8080);
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        
	// Bindeam el socket al port 8080
        int b_server = bind(s_server, (struct sockaddr*)&server_addr, sizeof(server_addr));
        
        if (b_server == -1)
        {
            perror("No s'ha pogut bindejar");
            if(destroy_torrent(&t_server) == -1)
              perror("No s'ha pogut destruir");
            return 1;
        }

	// Escoltam el socket, retornam -1 si no es pot escoltar
        int escolta = listen(s_server, SOMAXCONN);
        if (escolta == -1)
        {
            perror("No s'ha pogut escoltar");
            if(destroy_torrent(&t_server) == -1)
              perror("No s'ha pogut destruir");
            return 1;
        }

	// Mentres el servidor estigui actiu acceptam connexions
        while (1)
        {

            log_printf(LOG_INFO, "server: %d", s_server);
            socklen_t addr_len = sizeof(server_addr);

            int s1 = accept(s_server, (struct sockaddr*)&server_addr, &addr_len);
            if (s1 == -1)
            {
                perror("No s'ha acceptat");
                if(destroy_torrent(&t_server) == -1)
                  perror("No s'ha pogut destruir");
                return 1;
            }

            log_printf(LOG_INFO, "server_return: %d", s1);

	    //fork() fill i pare per a cada connexio, -1 no ha pogut crear-se, 0 fill, 1 pare
            
            int pid = fork();
            
            if (pid == -1)
            {
                perror("No s'ha pogut fer el fork");
                if(destroy_torrent(&t_server) == -1)
                  perror("No s'ha pogut destruir");
                return 1;
            }
            else if (pid == 0)
            {
                //fill
                close(s_server);
                uint8_t buffer_server[RAW_MESSAGE_SIZE];

		// Llegim el missatge del client
                while (recv(s1, buffer_server, sizeof(buffer_server), MSG_WAITALL) > 0)  // recv sigui diferent a 0 de s1)
                {

                    if (buffer_server[4] == MSG_REQUEST)
                    {
                        uint64_t num_bloc = 0;

			// Llegim el numero de bloc que ens envia el client
                        for (int b = 5; b < RAW_MESSAGE_SIZE; b++)
                        {
                            num_bloc |= (num_bloc << 8);
                            num_bloc |= buffer_server[b];
                        }

                        log_printf(LOG_INFO, "numero: %" PRId8, num_bloc);
                        struct block_t bloc_server;

			// Llegim el bloc del fitxer
                        int lb_server = load_block(&t_server, num_bloc, &bloc_server);
                        if (lb_server == -1)
                        {
                            perror("No s'ha pogut carregar");
                            if(destroy_torrent(&t_server) == -1)
                                perror("No s'ha pogut destruir");
                            return 1;
                        }

                        uint8_t bserver_missatge[bloc_server.size + RAW_MESSAGE_SIZE];
                        for (uint64_t i = 0; i < bloc_server.size + RAW_MESSAGE_SIZE; i++)
                        {
                            if (i < RAW_MESSAGE_SIZE)
                            {

                                if (i != 4)
                                    bserver_missatge[i] = buffer_server[i];

                                else
                                    bserver_missatge[i] = MSG_RESPONSE_OK;
                            }
                            else
                            {
                                bserver_missatge[i] = bloc_server.data[i - 13];
                            }

                        }

			// Enviem el bloc al client
                        ssize_t send_server = send(s1, (uint8_t*)bserver_missatge, sizeof(bserver_missatge), 0);
                        if (send_server == -1)
                        {
                            perror("No s'ha pogut enviar");
                            if(destroy_torrent(&t_server) == -1)
                                perror("No s'ha pogut destruir");
                            return 1;
                        }
                    }
                }


            }
            else
            {
		// Aquest és el pare
                wait(&pid);
                close(s1);
            }
        }

    }
    else
    {

        //CLIENT

        log_printf(LOG_INFO, "######################################################");
        log_printf(LOG_INFO, "###################ENTRANT CLIENT#####################");
        log_printf(LOG_INFO, "######################################################");
        
        
	// Carregam el torrent t_client a partir de l'arxiu .torrent
        struct torrent_t ttorrent;
        int ct = create_torrent_from_metainfo_file(argv[1], &ttorrent, "torrent_samples/client/test_file");
        if (ct == -1)
        {
            perror("No s'ha pogut crear");
            if(destroy_torrent(&ttorrent) == -1)
                  perror("No s'ha pogut destruir");
            return 1;
        }

	// Cream el socket i comprovam que es pugui crear, iteram els peers
        for (uint64_t i = 0; i < ttorrent.peer_count; i++)
        {
            int client_socket = socket(AF_INET, SOCK_STREAM, 0);

            if (client_socket == -1)
            {
                perror("No s'ha pogut crear el socket");
                if(destroy_torrent(&ttorrent) == -1)
                	perror("No s'ha pogut destruir");
                	
                return 1;
            }

	    // Cream l'adreça del client

            uint32_t p_addr = (uint32_t)ttorrent.peers[i].peer_address[0] << 24 | (in_addr_t)ttorrent.peers[i].peer_address[1] << 16 | (in_addr_t)ttorrent.peers[i].peer_address[2] << 8 | (in_addr_t)ttorrent.peers[i].peer_address[3] << 0;
            struct sockaddr_in peer_addr;
            peer_addr.sin_family = AF_INET;
            peer_addr.sin_port = ttorrent.peers[i].peer_port;
            peer_addr.sin_addr.s_addr = htonl(p_addr);

	    // Connect() al socket, retornam -1 si no es pot connectar
            
            int connectar = connect(client_socket, (struct sockaddr*)&peer_addr, sizeof(peer_addr));
            if (connectar == -1)
            {
                perror("No s'ha pogut connectar");

                if (close(client_socket) == -1)
                {
                    perror("No s'ha pogut tancar");
                    continue;
                }


            }

	    // Enviem el missatge al servidor, iteram els blocs 
            for (uint64_t j = 0; j < ttorrent.block_count; j++)
            {
                if (ttorrent.block_map[j] == MSG_RESPONSE_OK)
                    continue;

                struct block_t bloc;
                bloc.size = get_block_size(&ttorrent, j);
                
                assert(RAW_MESSAGE_SIZE >= 0);
                assert(MAX_BLOCK_SIZE >= 0);
                uint8_t buffer[RAW_MESSAGE_SIZE];
                uint8_t buffer2[RAW_MESSAGE_SIZE + MAX_BLOCK_SIZE];

                memset(buffer, 0, RAW_MESSAGE_SIZE);

                // Inicialitzem el buffer amb els parametres demanats utilizant bit shifting

                buffer[0] = (uint8_t)(MAGIC_NUMBER >> 24);
                buffer[1] = (uint8_t)(MAGIC_NUMBER >> 16);
                buffer[2] = (uint8_t)(MAGIC_NUMBER >> 8);
                buffer[3] = (uint8_t)(MAGIC_NUMBER >> 0);
                buffer[4] = MSG_REQUEST;
                buffer[5] = (uint8_t)(j >> 56);
                buffer[6] = (uint8_t)(j >> 48);
                buffer[7] = (uint8_t)(j >> 40);
                buffer[8] = (uint8_t)(j >> 32);
                buffer[9] = (uint8_t)(j >> 24);
                buffer[10] = (uint8_t)(j >> 16);
                buffer[11] = (uint8_t)(j >> 8);
                buffer[12] = (uint8_t)(j >> 0);

                
                ssize_t enviar = send(client_socket, buffer, sizeof(buffer), 0);
                if (enviar == -1)
                {
                    perror("No s'ha pogut enviar");
                    if(destroy_torrent(&ttorrent) == -1)
                    	perror("No s'ha pogut destruir");
                    return 1;
                }
		
                ssize_t comprovar = recv(client_socket, buffer2, RAW_MESSAGE_SIZE + bloc.size, MSG_WAITALL);
                if (comprovar == -1)
                {
                    perror("No s'ha pogut rebre");
                    if(destroy_torrent(&ttorrent) == -1)
                    	perror("No s'ha pogut destruir");
                    return 1;
                }

                // Inicialitzem el magic number amb les adreces del buffer 
                uint32_t magic_number = ((uint32_t)buffer2[0] << 24 | (uint32_t)buffer2[1] << 16 | (uint32_t)buffer2[2] << 8 | (uint32_t)buffer2[3]);

                // Comprovacions pel magic number
                //printf("El valor de 'magic_number' es: %u\n", magic_number);
                //printf("El valor de 'MAGIC_NUMBER' es: %u\n", MAGIC_NUMBER);

		// Comprovam que el magic number sigui correcte i que tinguem carregat el bloc
                if (magic_number == MAGIC_NUMBER && buffer2[4] == MSG_RESPONSE_OK)
                {
                    for (uint64_t k = 13; k < sizeof(buffer2); k++)
                        bloc.data[k - 13] = buffer2[k];

                    // Guardam el bloc del torrent i bloc que estem iterant
                    int storeb = store_block(&ttorrent, j, &bloc);
                    if (storeb == -1)
                    {
                        perror("No s'ha pogut guardar");
                        if(destroy_torrent(&ttorrent) == -1)
                        	perror("No s'ha pogut destruir");
                        return 1;
                    }

                }
            }
            // Tancam el client i sortim del client
         
            close(client_socket);
        }
    }

    // The following statements most certainly will need to be deleted at some point...
    (void)argc;
    //(void) argv;
    //(void) MAGIC_NUMBER;
    //(void) MSG_REQUEST;
    (void)MSG_RESPONSE_NA;
    //(void) MSG_RESPONSE_OK;


    return 0;
}
