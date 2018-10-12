#ifndef P2PSERVER_H
#define P2PSERVER_H

#include "../node/P2PPeerNode.cpp"

using namespace std;

class P2PServer
{
	private:
		void initialize();
		void runProgram();
		void handleRequest(int, string);
		string addFiles(int, vector<string>);
		string listFiles();
		void updateFileList();
		bool socketsModified();
		string getFile(vector<string>);

		bool hasFileWithId(int);
		FileItem getFileItem(int);

		bool hasFileItemWithNameSize(string, int);
		FileItem getFileItemWithNameSize(string, int);
		void updateFileItem(FileItem);

		bool isAddressAttachedToFileItem(FileAddress, FileItem);
		bool isSocketAttachedToFileItem(int, FileItem);

		// Server limits and port
		int PORT_NUMBER;

		// Thread worker functions
		static void * startActivityListenerThread(void *);

		// Keep track of the peer node
		P2PPeerNode node;

		// Keep a list of the files and active clients
		vector<FileItem> file_list;
		timeval sockets_last_modified;
		int max_file_id;

	public:
		P2PServer();
		void start();
};

#endif