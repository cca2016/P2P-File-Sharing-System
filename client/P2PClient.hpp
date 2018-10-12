#ifndef P2PCLIENT_H
#define P2PCLIENT_H

#include "../node/P2PPeerNode.cpp"

using namespace std;

class P2PClient
{
	private:
		void initialize();

		// UI Methods
		void runProgram();
		bool runUI();
		char showMenu();
		void viewFiles();
		void selectFiles();
		vector<FileItem> collectFiles(vector<string>);
		void sendFiles(vector<FileItem>);
		void getFile();
		void showProgress();

		// UI Management
		bool b_awaiting_response;

		// Thread worker functions
		static void * startActivityListenerThread(void *);
		static void * startTransferThread(void *);

		// Keep track of the peer node
		P2PPeerNode node;
		int server_socket;

		// File transfer
		static const unsigned int FILE_CHUNK_SIZE = 224;
		static const unsigned int HEADER_SIZE = 63;

		// File List
		vector<FileItem> local_file_list;
		void saveFileList(vector<FileItem>);
		bool hasLocalFile(string, int);
		bool getLocalFile(string, int);

	public:
		P2PClient();
		void start(string, int);
};

#endif