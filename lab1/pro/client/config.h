#define _WINSOCK_DEPRECATED_NO_WARNINGS 1	// enable deprecated functions
#define SERVERADDRESS "127.0.0.1"
#define PORT 8888
#define BUFFER_SIZE 1024

#define COMMAND_LEN 4

#define COMM_QUIT "QUIT"	// QUIT
#define COMM_HELP "HELP"	// HELP
#define COMM_NAME "NAME"	// NAME [new name]
#define COMM_SEND "SEND"	// SEND [receive id]:[message]
#define COMM_BROA "BROA"	// BROA:[message]
#define COMM_LIST "LIST"	// LIST
#define RECV_IDEN "IDEN"	// IDEN ID:[id]
#define RECV_SEND "SEND"	// SEND [from id]:[message]
#define RECV_BROA "BROA"	// BROA [from id]:[message]