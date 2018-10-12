/**
 *Peer-to-peer server class
 */

#include "P2PServer.hpp"

P2PServer::P2PServer()
{
	max_file_id = 0;
}

void P2PServer::start()
{
	// Open the socket and listen for connections
	node = P2PPeerNode(27890, 512);
	node.setBindMaxOffset(1);
	node.start();

	// Create a separate thread to connect & bind
	pthread_t node_thread;
	if (pthread_create(&node_thread, NULL, &P2PServer::startActivityListenerThread, (void *)&node) != 0)
	{
		perror("Error: could not spawn peer listeners");
		exit(1);
	}

	runProgram();
}

void * P2PServer::startActivityListenerThread(void * arg)
{
	P2PPeerNode * node;
	node = (P2PPeerNode *) arg;
	node->listenForActivity();
	pthread_exit(NULL);
}

void P2PServer::runProgram()
{
	bool b_program_active = true;
	while (b_program_active)
	{
		// Handle updates
		if (socketsModified())
		{
			// Update socket-dependent logic
			updateFileList();
		}

		if (node.countQueueMessages() > 0)
		{
			// Get the oldest message
			P2PMessage message = node.popQueueMessage();

			// Handle message
			handleRequest(message.socket_id, message.message);
		}
		else
		{
			usleep(5000);
		}
	}
}

bool P2PServer::socketsModified()
{
	timeval node_sockets_lm = node.getSocketsLastModified();
	return (sockets_last_modified.tv_sec != node_sockets_lm.tv_sec || 
			sockets_last_modified.tv_usec != node_sockets_lm.tv_usec);
}

void P2PServer::updateFileList()
{
	// Get all of the current sockets
	vector<P2PSocket> sockets = node.getSockets();

	// Prep work
	bool b_socket_found;
	vector<FileItem>::iterator file_iter;
	vector<FileAddress>::iterator addr_iter;
	vector<P2PSocket>::iterator sock_iter;

	// For each file, ensure that the file's owners are online
	for (file_iter = file_list.begin(); file_iter < file_list.end(); )
	{
		b_socket_found = false;

		// Validate each address attached to each file
		for (addr_iter = (*file_iter).addresses.begin(); addr_iter < (*file_iter).addresses.end(); )
		{
			for (sock_iter = sockets.begin(); sock_iter < sockets.end(); sock_iter++)
			{
				if ((*sock_iter).socket_id == (*addr_iter).socket_id)
				{
					b_socket_found = true;
					break;
				}
			}

			if (b_socket_found)
				addr_iter++;
			else
				(*file_iter).addresses.erase(addr_iter);
		}

		// If a file has no more addresses attached to it, then remove it
		if ((*file_iter).addresses.size() > 0)
			file_iter++;
		else
			file_list.erase(file_iter);
	}

	// Update the last modified time
	sockets_last_modified = node.getSocketsLastModified();
}

void P2PServer::handleRequest(int socket, string request)
{
	// Parse the request
	vector<string> request_parsed = P2PCommon::parseRequest(request);

	// Trim whitespace from the command
	request_parsed[0] = P2PCommon::trimWhitespace(request_parsed[0]);

	// Report to the front
	cerr << request_parsed[0] << endl;

	// Parse the request for a matching command
	if (request_parsed[0].compare("addFiles") == 0)
	{
		cerr << "Adding files" << endl;

		string message = addFiles(socket, request_parsed);
		write(socket, message.c_str(), message.length());
	}
	else if (request_parsed[0].compare("list") == 0)
	{
		cerr << "Listing files" << endl;

		string files = listFiles();
		write(socket, files.c_str(), files.length());
	}
	else if (request_parsed[0].compare("getFile") == 0)
	{
		cerr << "Getting file" << endl;

		string message = getFile(request_parsed);
		write(socket, message.c_str(), message.length());
	}
	else
	{
		cerr << "Request unknown: " << request << endl;
	}
}

string P2PServer::addFiles(int socket, vector<string> files)
{
	// Get the public address / port
	vector<string> address = P2PCommon::parseAddress(files[1]);

	// Get the socket's IP address
	struct sockaddr_in client_address = node.getClientAddressFromSocket(socket);
	string client_public_address = inet_ntoa(client_address.sin_addr);

	int i = 0;
	vector<string>::iterator iter;
	for (iter = files.begin() + 2; iter < files.end(); iter++, i++)
	{
		stringstream line(*iter);
		string segment;
		vector<string> seglist;
		while (getline(line, segment, '\t'))
		{
			seglist.push_back(segment);
		}

		// Create the new FileAddress record
		FileAddress file_address;
		file_address.remote_path = seglist[2];
		file_address.socket_id = socket;
		file_address.public_address = client_public_address;
		file_address.public_port = stoi(address[1]);

		// If the file is new, add a new record
		FileItem file_item;
		if (hasFileItemWithNameSize(seglist[0], stoi(seglist[1])))
		{
			file_item = getFileItemWithNameSize(seglist[0], stoi(seglist[1]));
		}
		else
		{
			file_item.name = seglist[0];
			file_item.size = stoi(seglist[1]);
			file_item.file_id = ++max_file_id;

			file_list.push_back(file_item);
		}

		// Don't add the address if we already have it
		if (!isAddressAttachedToFileItem(file_address, file_item))
		{
			// Add the address to the list
			file_item.addresses.push_back(file_address);

			// Update the file item record
			updateFileItem(file_item);
		}
	}

	return to_string(i) + " files successfully added to file listing.";
}

void P2PServer::updateFileItem(FileItem file_item)
{
	vector<FileItem>::iterator iter;
	for (iter = file_list.begin(); iter < file_list.end(); iter++)
	{
		if (file_item.file_id == (*iter).file_id)
		{
			(*iter) = file_item;
			break;
		}
	}
}

string P2PServer::listFiles()
{
	if (file_list.size() == 0)
	{
		return "\r\nThere are currently no files stored on the server.\r\n";
	}

	string files_message = "\r\nFile Listing:\r\n";

	vector<FileItem>::iterator iter;
	for (iter = file_list.begin(); iter < file_list.end(); iter++)
	{
		files_message += "\t" + to_string((*iter).file_id) + ") " + (*iter).name + " - (" + to_string((*iter).size) + " B)" "\r\n";
	}

	return files_message;
}

string P2PServer::getFile(vector<string> request)
{
	// Get the File Item info from the file ID
	int file_id = stoi(request[1]);

	// Validate that the file exists
	if (!hasFileWithId(file_id))
	{
		return "fileAddress\r\nNULL\r\nNULL\r\nNULL\r\nNULL";
	}

	// Get the file item
	FileItem file_item = getFileItem(file_id);

	// Compile all of the public addresses
	string address_list;
	vector<FileAddress>::iterator iter;
	for (iter = file_item.addresses.begin(); iter < file_item.addresses.end(); iter++)
	{
		address_list += "\r\n" + (*iter).public_address + ":" + to_string((*iter).public_port);
	}

	// Report the disconnection
	return "fileAddress\r\n" + to_string(file_id) + "\r\n"
			+ file_item.name + "\r\n" + to_string(file_item.size)
			+ address_list;
}

bool P2PServer::hasFileItemWithNameSize(string name, int size)
{
	vector<FileItem>::iterator iter;
	for (iter = file_list.begin(); iter < file_list.end(); iter++)
	{
		if ((*iter).size == size && (*iter).name.compare(name) == 0)
		{
			return true;
		}
	}

	return false;
}

bool P2PServer::hasFileWithId(int file_id)
{
	vector<FileItem>::iterator iter;
	for (iter = file_list.begin(); iter < file_list.end(); iter++)
	{
		if ((*iter).file_id == file_id)
		{
			return true;	
		}
	}

	return false;
}

FileItem P2PServer::getFileItemWithNameSize(string name, int size)
{
	FileItem file_item;
	vector<FileItem>::iterator iter;
	for (iter = file_list.begin(); iter < file_list.end(); iter++)
	{
		if ((*iter).size == size && (*iter).name.compare(name) == 0)
		{
			file_item = (*iter);
			break;
		}
	}

	return file_item;
}

FileItem P2PServer::getFileItem(int file_id)
{
	FileItem file_item;
	vector<FileItem>::iterator iter;
	for (iter = file_list.begin(); iter < file_list.end(); iter++)
	{
		if ((*iter).file_id == file_id)
		{
			file_item = *iter;
			break;
		}
	}

	return file_item;
}

bool P2PServer::isAddressAttachedToFileItem(FileAddress file_address, FileItem file_item)
{
	vector<FileAddress>::iterator iter;
	for (iter = file_item.addresses.begin(); iter < file_item.addresses.end(); iter++)
	{
		if ((*iter).public_address == file_address.public_address &&
			(*iter).public_port == file_address.public_port)
		{
			return true;
		}
	}

	return false;
}

bool P2PServer::isSocketAttachedToFileItem(int socket_id, FileItem file_item)
{
	vector<FileAddress>::iterator iter;
	for (iter = file_item.addresses.begin(); iter < file_item.addresses.end(); iter++)
	{
		if ((*iter).socket_id == socket_id)
		{
			return true;
		}
	}

	return false;
}

