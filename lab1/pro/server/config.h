#define _WINSOCK_DEPRECATED_NO_WARNINGS 1	// enable deprecated functions
#define SERVERADDRESS "127.0.0.1"
#define PORT 8888
#define BUFFER_SIZE 1024

#define COMMAND_LEN 4

#define ONLINE true
#define OFFLINE false

#define COMM_QUIT "QUIT"	// QUIT
#define COMM_HELP "HELP"	// HELP
#define COMM_IDEN "IDEN"	// IDEN [id] 
#define COMM_NAME "NAME"	// NAME [id]:[new name]
#define COMM_SEND "SEND"	// SEND [receive id]:[message]
#define COMM_BROA "BROA"	// BROA [from id]:[message]
#define COMM_LIST "LIST"	// LIST
#define COMM_KICK "KICK"	// KICK [id]

#define RECV_SEND "SEND"	// SEND [id]:[message]
#define RECV_BROA "BROA"	// BROA [id]:[message]
#define RECV_NAME "NAME"	// NAME [id]:[new name]    send id is eliminated cause specific is handling the socket

const char helpTxt[1024] = "This is how you can use s-c chatroom:\n\t QUIT: quit the chatroom\n\t HELP: show this help\n\t IDEN [id]: identify yourself\n\t NAME [id]:[new name]: change your name\n\t SEND [receive id]:[message]: send message to specific user\n\t BROA [from id]:[message]: broadcast message to all users\n\t LIST: list all users\n\t KICK [id]: kick specific user out of the chatroom";
