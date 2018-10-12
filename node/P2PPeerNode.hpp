#ifndef P2PPEERNODE_H
#define P2PPEERNODE_H

#include "../common/P2PCommon.cpp"
#include "../filetransfer/P2PFileTransfer.cpp"

using namespace std;

class P2PPeerNode
{
	private:
		void construct(int, int);
		void initialize();
		void resetSocketDescriptors();
		void handleNewConnectionRequest();
		void handleExistingConnections();
		void handleRequest(int, char*);
		void enqueueMessage(int, char*);

		// Get File for transfer
		void prepareFileTransferRequest(vector<string>);
		void prepareFileTransferRequest(vector<string>, int, int);

		// Stub functions
		FileItem getFileItem(vector<FileItem>&, int);
		void addFileItems(vector<FileItem>&, vector<FileItem>);
		bool hasFileItem(vector<FileItem>&, string, int);
		FileItem getFileItem(vector<FileItem>&, string, int);

		FileItem getLocalFileItem(int);
		FileItem getDownloadFileItem(int);

		// Worker threads
		static void * initiateFileTransfer(void *);
		static void * handleFileTransfer(void *);

		// Own socket
		void openPrimarySocket();
		int  bindPrimarySocket(int, int);

		// Add a file to the server
		void addFileToServer(FileItem);

		// Server limits and port
		int PORT_NUMBER;
		int MAX_CONNECTIONS;
		int BUFFER_SIZE;
		int INCOMING_MESSAGE_SIZE;

		// Buffer
		char * buffer;

		// Message queue and file list
		vector<P2PMessage> message_queue;
		vector<FileItem> local_file_list;
		vector<FileItem> download_file_list;

		// Sockets
		int primary_socket;
		int * sockets;
		int max_connection;

		// Managing Sockets
		timeval sockets_last_modified;

		// Available sockets
		vector<P2PSocket> socket_vector;
		vector<int> sockets_to_close;

		// Settings
		int port_offset;
		int public_port;
		unsigned int number_bind_tries;

		// Socket descriptors used for select()
		fd_set socket_descriptors;

	public:
		P2PPeerNode();
		P2PPeerNode(int, int);
		void start();
		void listenForActivity();
		void monitorTransfers();
		void setBindMaxOffset(unsigned int);

		// Add and remove new connections
		int makeConnection(string, string, int);
		void closeSocket(int);
		void closeSocketByName(string);
		void queueSocketToClose(int);
		void queueSocketToCloseByName(string);

		int countSockets();
		vector<P2PSocket> getSockets();
		timeval getSocketsLastModified();
		struct sockaddr_in getClientAddressFromSocket(int);
		struct sockaddr_in getPrimaryAddress();
		int getPublicPort();

		bool hasSocketByName(string);
		P2PSocket getSocketByName(string);
		void sendMessageToSocketName(string, string);

		// Progress
		string getFileProgress();
		string analyzeFileProgress(FileItem);

		// Send message to socket
		void sendMessageToSocket(string, int);
		void requestFileTransfer(string, int, string, int, unsigned int, unsigned int);

		// Interact with message queue
		P2PMessage popQueueMessage();
		int countQueueMessages();

		// Handle download files
		void addDownloadFileItems(vector<FileItem>);
		bool hasDownloadFileItem(string, int);
		FileItem getDownloadFileItem(string, int);

		// Handle local files
		void addLocalFileItems(vector<FileItem>);
		bool hasLocalFileItem(string, int);
		FileItem getLocalFileItem(string, int);
};

#endif