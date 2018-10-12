#ifndef P2PFILETRANSFER_H
#define P2PFILETRANSFER_H

using namespace std;

class P2PFileTransfer
{
	private:
		vector<FileItem> file_list;
		unsigned int start;
		unsigned int count;

	public:
		P2PFileTransfer();

		void setBounds(unsigned int, unsigned int);
		void startTransferFile(FileItem, int);
		void handleIncomingFileTransfer(FileDataPacket);
		string computeChecksum(char *, int);
		void reviewTransfers(vector<FileItem>&);
		bool compileFileParts(FileItem&);

		// File transfer
		static const unsigned int FILE_CHUNK_SIZE = 449;
		static const unsigned int HEADER_SIZE = 63;

		// Data folder
		static const string DATA_FOLDER;
};

#endif