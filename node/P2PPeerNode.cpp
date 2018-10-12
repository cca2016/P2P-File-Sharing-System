/**
 * Peer-to-peer node class
 */

#include "P2PPeerNode.hpp"

/**
 * Public Methods
 */

P2PPeerNode::P2PPeerNode(int port, int max_connections_value)
{
	this->construct(port, max_connections_value);
}

P2PPeerNode::P2PPeerNode()
{
	this->construct(27891, 512);
}

void P2PPeerNode::construct(int port, int max_connections_value)
{
	// Define the port number to listen through
	PORT_NUMBER = port;

	// Define server limits
	MAX_CONNECTIONS = max_connections_value;
	BUFFER_SIZE = 513; // Size given in bytes
	INCOMING_MESSAGE_SIZE = BUFFER_SIZE - 1;

	// Keep track of the client sockets
	sockets = new int[MAX_CONNECTIONS];

	// Handle the buffer
	buffer = new char[BUFFER_SIZE];

	// Default bind offset
	number_bind_tries = 1;

	// Keep track of the last update to the sockets
	gettimeofday(&sockets_last_modified, NULL);
}

void P2PPeerNode::setBindMaxOffset(unsigned int max_offset)
{
	number_bind_tries = max_offset;
}

//void P2PPeerNode::setBindPort(string address) {}
//void P2PPeerNode::setMaxClients(string address) {}

void P2PPeerNode::start()
{
	// Open the socket and listen for connections
	this->initialize();
	this->openPrimarySocket();
}


/**
 * Private Methods
 */
void P2PPeerNode::initialize()
{
	// Ensure that all sockets are invalid to boot
	for (int i = 0; i < MAX_CONNECTIONS; i++)
	{
		sockets[i] = 0;
	}
}

/**
 * Primary Socket
 * - Open and bind a socket to serve
 */

void P2PPeerNode::openPrimarySocket()
{
	// Create the socket - use SOCK_STREAM for TCP, SOCK_DGRAM for UDP
	primary_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (primary_socket < 0)
	{
		perror("Error: could not open primary socket");
		exit(1);
	}

	// Avoid the annoying "Address already in use" messages that the program causes
	int opt = 1;
	if (setsockopt(primary_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
    {
        perror("Error: could not use setsockopt to set 'SO_REUSEADDR'");
        exit(1);
    }

	// Bind the socket
	port_offset = 0;
	int socket_status = -1;
	while (socket_status < 0)
	{
		// Bind to the given socket
		socket_status = this->bindPrimarySocket(primary_socket, port_offset);

		// Stop eventually, if we can't bind to any ports
		if (socket_status < 0 && ++port_offset >= number_bind_tries)
		{
			perror("Fatal Error: Could not bind primary socket to any ports");
			exit(1);
		}
	}

	// Start listening on this port - second arg: max pending connections
	if (listen(primary_socket, MAX_CONNECTIONS) < 0)
	{
		perror("Error: could not listen on port");
		exit(1);
	}

	// Set the public-facing port
	public_port = PORT_NUMBER + port_offset;

	P2PSocket a_socket;
	a_socket.socket_id = primary_socket;
	a_socket.type = "primary";
	socket_vector.push_back(a_socket);

	// Keep track of the last update to the sockets
	gettimeofday(&sockets_last_modified, NULL);

	cout << "Listening on port " << (PORT_NUMBER + port_offset) << endl;
}

int P2PPeerNode::bindPrimarySocket(int socket, int offset)
{
	struct sockaddr_in server_address;

	// Clear out the server_address memory space
	memset((char *) &server_address, 0, sizeof(server_address));

	// Configure the socket information
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = INADDR_ANY;
	server_address.sin_port = htons(PORT_NUMBER + offset);

	return ::bind(socket, (struct sockaddr *) &server_address, sizeof(server_address));
}


/**
 * Make a connection
 */
int P2PPeerNode::makeConnection(string name, string host, int port)
{
	struct sockaddr_in server_address;
	struct hostent * server;

	// Create the socket - use SOCK_STREAM for TCP, SOCK_DGRAM for UDP
	int new_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (new_socket < 0)
	{
		cout << "Error: could not open socket" << endl;
		return -1;
	}

	// Find the server host
	server = gethostbyname(host.c_str());
	if (server == NULL)
	{
		cout << "Error: could not find the host" << endl;
		return -1;
	}

	// Clear out the server_address memory space
	memset((char *) &server_address, 0, sizeof(server_address));

	// Configure the socket information
	server_address.sin_family = AF_INET;
	memcpy(server->h_addr, &server_address.sin_addr.s_addr, server->h_length);
	server_address.sin_port = htons(port);

	// Connect to the server
	if (connect(new_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) 
	{
		perror("Error: could not connect to host");
		return -1;
	}

	// Add socket to open slot
	for (int i = 0; i <= MAX_CONNECTIONS; i++) 
	{
		// If we reach the maximum number of clients, we've gone too far
		// Can't make a new connection!
		if (i == MAX_CONNECTIONS)
		{
			// Report connection denied
			cout << "Reached maximum number of connections, can't add new one" << endl;
			close(new_socket);
			return -1;
		}

		// Skip all valid client sockets
		if (sockets[i] != 0) continue;

		// Add new socket
		sockets[i] = new_socket;

		P2PSocket a_socket;
		a_socket.socket_id = new_socket;
		a_socket.type = "server";
		a_socket.name = name;
		socket_vector.push_back(a_socket);

		// Keep track of the last update to the sockets
		gettimeofday(&sockets_last_modified, NULL);

		break;
	}

	cout << "Find IP of peers containing the target file ---"<< host << ':' << port << endl;

	return new_socket;
}

/*
bool P2PPeerNode::isConnection(string address, int port)
{
	for (iter = socket_vector.begin(); iter != socket_vector.end(); ++iter)
	{
		if (iter->public_address == address)
		{
			return true;
		}
	}

	return false;
}
*/

void P2PPeerNode::queueSocketToClose(int socket_id)
{
	sockets_to_close.push_back(socket_id);
}

void P2PPeerNode::queueSocketToCloseByName(string socket_name)
{
	P2PSocket socket = getSocketByName(socket_name);
	sockets_to_close.push_back(socket.socket_id);
}

void P2PPeerNode::closeSocketByName(string socket_name)
{
	// Remove the socket from the vector
	vector<P2PSocket>::iterator iter;
	for (iter = socket_vector.begin(); iter != socket_vector.end(); )
	{
		if (iter->name.compare(socket_name) == 0)
		{
			// Remove from the int array
			for (int i = 0; i < MAX_CONNECTIONS; i++)
			{
				if (iter->socket_id == sockets[i])
				{
					sockets[i] = 0;
					break;
				}
			}

			iter = socket_vector.erase(iter);

			// Close and free the socket
			close(iter->socket_id);
		}
		else
			++iter;
	}

	// Keep track of the last update to the sockets
	gettimeofday(&sockets_last_modified, NULL);
}

void P2PPeerNode::closeSocket(int socket)
{
	// Remove the socket from the vector
	vector<P2PSocket>::iterator iter;
	for (iter = socket_vector.begin(); iter != socket_vector.end(); )
	{
		if (iter->socket_id == socket)
			iter = socket_vector.erase(iter);
		else
			++iter;
	}

	// Keep track of the last update to the sockets
	gettimeofday(&sockets_last_modified, NULL);

	// Remove from the int array
	for (int i = 0; i < MAX_CONNECTIONS; i++)
	{
		if (socket == sockets[i])
		{
			sockets[i] = 0;
			break;
		}
	}

	// Close and free the socket
	close(socket);
}

/**
 * Handle Connection Activity
 */

void P2PPeerNode::resetSocketDescriptors()
{
	// Determine if we have any sockets to close
	while (sockets_to_close.size() > 0)
	{
		closeSocket(sockets_to_close.front());
		sockets_to_close.erase(sockets_to_close.begin());
	}

	// Reset the current socket descriptors
	FD_ZERO(&socket_descriptors);
	FD_SET(primary_socket, &socket_descriptors);

	// Keep track of the maximum socket descriptor for select()
	max_connection = primary_socket;

	// Add remaining sockets
	for (int i = 0; i < MAX_CONNECTIONS; i++)
	{
		// Validate the socket
		if (sockets[i] <= 0) continue;

		// Add socket to set
		FD_SET(sockets[i], &socket_descriptors);

		// Update the maximum socket descriptor
		max_connection = max(max_connection, sockets[i]);
	}
}

void P2PPeerNode::handleNewConnectionRequest()
{
	// Prepare the client address
	struct sockaddr_in client_address;
	socklen_t client_address_length = sizeof(client_address);

	// Accept a new socket
	int new_socket = accept(primary_socket, (struct sockaddr *)&client_address, &client_address_length);

	// Validate the new socket
	if (new_socket < 0)
	{
		perror("Error: failure to accept new socket");
		exit(1);
	}

	// Report new connection
	//cout << "New Connection Request: " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << endl;

	// Add socket to open slot
	for (int i = 0; i <= MAX_CONNECTIONS; i++)
	{
		// If we reach the maximum number of clients, we've gone too far
		// Can't accept a new connection!
		if (i == MAX_CONNECTIONS)
		{
			// Report connection denied
			cout << "Reached maximum number of clients, denied connection request" << endl;

			// Send refusal message to socket
			string message = "Server is too busy, please try again later\r\n";
			write(new_socket, message.c_str(), message.length());

			close(new_socket);
			break;
		}

		// Skip all valid client sockets
		if (sockets[i] != 0) continue;

		// Add new socket
		sockets[i] = new_socket;

		P2PSocket a_socket;
		a_socket.socket_id = new_socket;
		a_socket.type = "client";
		a_socket.name = "";
		socket_vector.push_back(a_socket);

		// Keep track of the last update to the sockets
		gettimeofday(&sockets_last_modified, NULL);

		break;
	}
}

void P2PPeerNode::handleExistingConnections()
{
	// Prepare the client address
	struct sockaddr_in client_address;
	socklen_t client_address_length = sizeof(client_address);

	// Iterate over all clients
	for (int i = 0; i < MAX_CONNECTIONS; i++) 
	{
		if (!FD_ISSET(sockets[i], &socket_descriptors)) continue;

		// Clear out the buffer
		memset(&buffer[0], 0, BUFFER_SIZE);

		// Read the incoming message into the buffer
		int message_size = read(sockets[i], buffer, INCOMING_MESSAGE_SIZE);

		// Handle a closed connection
		if (message_size == 0)
		{
			// Report the disconnection
			getpeername(sockets[i], (struct sockaddr*)&client_address, &client_address_length);
			//cout << "Connection closed: " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << endl;

			// Close and free the socket
			//queueSocketToClose(sockets[i]);
			closeSocket(sockets[i]);
		}
		else
		{
			// Make a copy of the vector to avoid concurrency issues
			vector<P2PSocket> socket_vector_copy;
			socket_vector_copy = socket_vector;

			// Remove the socket from the vector
			vector<P2PSocket>::iterator iter;
			for (iter = socket_vector_copy.begin(); iter != socket_vector_copy.end(); ++iter)
			{
				if (iter->socket_id == sockets[i])
				{
					// Parse the request
					vector<string> request_parsed = P2PCommon::parseRequest(buffer);

					// Trim whitespace from the command
					request_parsed[0] = P2PCommon::trimWhitespace(request_parsed[0]);

					if (request_parsed[0].compare("fileTransfer") == 0)
					{
						// Get the File ID
						vector<string> file_id_info = P2PCommon::splitString(request_parsed[1], ':');
						vector<string> header_info = P2PCommon::splitString(request_parsed[1], '\t');
						int file_id = stoi(P2PCommon::trimWhitespace(header_info[0]));

						// Make a copy of the data
						char * buffer_copy = new char[BUFFER_SIZE];
						memcpy(buffer_copy, buffer, BUFFER_SIZE);

						// Pack the data neatly for travel
						FileDataPacket packet;
						packet.file_item = getDownloadFileItem(file_id);
						packet.packet = buffer_copy;

						// Perform this as a separate thread
						pthread_t thread;
						if (pthread_create(&thread, NULL, &P2PPeerNode::handleFileTransfer, (void *)&packet) != 0)
						{
							perror("Error: could not spawn thread");
							//exit(1);
						}
					}
					//else if (request_parsed[0].compare("initiateFileTransfer") == 0)
					else if (request_parsed[0].compare("fileRequest") == 0)
					{
						// Get the File ID
						int file_id = stoi(P2PCommon::trimWhitespace(request_parsed[1]));
						string name = P2PCommon::trimWhitespace(request_parsed[2]);
						int size = stoi(P2PCommon::trimWhitespace(request_parsed[3]));
						unsigned int start = stoi(P2PCommon::trimWhitespace(request_parsed[4]));
						unsigned int count = stoi(P2PCommon::trimWhitespace(request_parsed[5]));

						// Prepare the request
						FileDataRequest request;
						request.socket_id = iter->socket_id;
						request.start = start;
						request.count = count;
						request.file_item = getLocalFileItem(name, size);

						// Set the file ID
						request.file_item.file_id = file_id;

						// Perform this as a separate thread
						pthread_t thread;
						if (pthread_create(&thread, NULL, &P2PPeerNode::initiateFileTransfer, (void *)&request) != 0)
						{
							perror("Error: could not spawn thread");
							//exit(1);
						}

						// Wait for the thread to obtain access to the data without freeing the memory
						sleep(1);
					}
					else if (request_parsed[0].compare("fileAddress") == 0)
					{
						prepareFileTransferRequest(request_parsed);
					}
					else if (iter->type.compare("server") == 0)
					{
						this->enqueueMessage(sockets[i], buffer);
					}
					else if (iter->type.compare("client") == 0)
					{
						this->enqueueMessage(sockets[i], buffer);
					}
				}
			}
		}
	}
}

void P2PPeerNode::listenForActivity()
{
	while (true) 
	{
		// Ready the socket descriptors
		this->resetSocketDescriptors();
  
		// Wait for activity
		int activity = select(max_connection + 1, &socket_descriptors, NULL, NULL, NULL);

		// Validate the activity
		if ((activity < 0) && (errno!=EINTR))
		{
			perror("Error: select failed");
			exit(1);
		}

		// Anything on the primary socket is a new connection
		if (FD_ISSET(primary_socket, &socket_descriptors))
		{
			this->handleNewConnectionRequest();
		}

		// Perform any open activities on all other clients
		this->handleExistingConnections();
	}
}

void P2PPeerNode::monitorTransfers()
{
	P2PFileTransfer file_transfer;
	map<int, int> file_to_missing_piece;

	while (true)
	{
		// Give it a rest
		sleep(3);

		// Check to see how file transfers are doing.
		// If any get stuck, make a request to download more parts.	
		file_transfer.reviewTransfers(download_file_list);

		// If any are newly completed, remove them from the list
		vector<FileItem>::iterator iter;
		for (iter = download_file_list.begin(); iter < download_file_list.end(); )
		{
			// Make sure to register newly finished files with the server
			// Also remove from the downloads list, add to the local list
			if ((*iter).completed)
			{
				FileItem copy_file_item;
				copy_file_item.file_id = (*iter).file_id;
				copy_file_item.name = (*iter).name;
				copy_file_item.size = (*iter).size;
				copy_file_item.path = (*iter).path; // This was updated when the pieces were put together

				local_file_list.push_back(copy_file_item);
				addFileToServer(*iter);
				iter = download_file_list.erase(iter);

				// Also disconnect when we're done
				queueSocketToCloseByName(to_string(copy_file_item.file_id));
			}
			else if ((*iter).missing_pieces.size() > 0 && !file_to_missing_piece[(*iter).file_id])
			{
				// We have missing pieces, but this is the first time we've noticed it
				// So, keep track of the file with missing pieces, and compare the next go-around
				file_to_missing_piece[(*iter).file_id] = (*iter).missing_pieces[0];
				iter++;
			}
			else if ((*iter).missing_pieces.size() > 0 
				&& file_to_missing_piece[(*iter).file_id] == (*iter).missing_pieces[0])
			{
				// We have missing pieces that have persisted - time to send a new request
				//queueSocketToCloseByName(to_string((*iter).file_id));
				sendMessageToSocketName("central_server", "getFile\r\n" + to_string((*iter).file_id));

				file_to_missing_piece[(*iter).file_id] = 0;
				//iter = download_file_list.erase(iter);
				iter++;
			}
		}
	}
}


void P2PPeerNode::addFileToServer(FileItem file_item)
{
	// Prepare the primary address
	struct sockaddr_in primary_address;
	socklen_t primary_address_length = sizeof(primary_address);

	// Get the public-facing port and IP address
	primary_address = getPrimaryAddress();
	string address = inet_ntoa(primary_address.sin_addr);

	string add_files_message = "addFiles\r\n" + address + ":" + to_string(getPublicPort()) + "\r\n";
	add_files_message += file_item.name + '\t' + to_string(file_item.size) + '\t' + file_item.path + "\r\n";

	sendMessageToSocketName("central_server", add_files_message);
}

/**
 * Program Logic
 */

void P2PPeerNode::sendMessageToSocket(string request, int socket)
{
	// Write the message to the server socket
	if (write(socket, request.c_str(), request.length()) < 0)
	{
		perror("Error: could not send message to server");
		exit(1);
	}
}

void P2PPeerNode::enqueueMessage(int client_socket, char* buffer)
{
	P2PMessage message;
	message.socket_id = client_socket;
	message.message = string(buffer);

	// Push to the queue
	message_queue.push_back(message);
}

int P2PPeerNode::countSockets()
{
	return socket_vector.size();
}

timeval P2PPeerNode::getSocketsLastModified()
{
	return sockets_last_modified;
}

vector<P2PSocket> P2PPeerNode::getSockets()
{
	return socket_vector;
}

struct sockaddr_in P2PPeerNode::getClientAddressFromSocket(int socket_id)
{
	// Prepare the client address
	struct sockaddr_in client_address;
	socklen_t client_address_length = sizeof(client_address);

	// Get the peer name
	getpeername(socket_id, (struct sockaddr*)&client_address, &client_address_length);

	return client_address;
}

struct sockaddr_in P2PPeerNode::getPrimaryAddress()
{
	return getClientAddressFromSocket(primary_socket);
}

int P2PPeerNode::getPublicPort()
{
	return public_port;
}

int P2PPeerNode::countQueueMessages()
{
	return message_queue.size();
}

P2PMessage P2PPeerNode::popQueueMessage()
{
	// Validation
	if (message_queue.size() <= 0)
	{
		perror("Error: can't pop a message from an empty queue");
		exit(1);
	}

	// Return a message
	P2PMessage message = message_queue[0];
	message_queue.erase(message_queue.begin());
	return message;
}

string P2PPeerNode::getFileProgress()
{
	string progress;

	// Iterate through all files in the local file list
	vector<FileItem>::iterator iter;
	for (iter = download_file_list.begin(); iter != download_file_list.end(); ++iter)
	{
		// RBH need to get actual progress
		progress += "\t" + (*iter).name + "\t" + analyzeFileProgress((*iter)) + "\r\n";
	}

	if (progress.length() == 0)
	{
		progress = "You have no active file transfers.";
	}

	return progress;
}

string P2PPeerNode::analyzeFileProgress(FileItem file_item)
{
	// First, check to see if the file is finished downloading
	DIR *directory;
	struct dirent *ep;

	// Open the directory
	directory = opendir(P2PFileTransfer::DATA_FOLDER.c_str());
	if (directory != NULL)
	{
		// Get the expected number of parts
		unsigned int size_downloaded = 0;

		string filename = to_string(file_item.file_id) + ".pt.";

		// See how many parts we have available
		while ((ep = readdir(directory)) != NULL)
		{
			// Evaluate that each part is there
			string this_filename = string(ep->d_name);
			if (this_filename.compare(0, filename.length(), filename) == 0
				&& this_filename.compare(this_filename.length() - 6, 6, ".p2pft") == 0)
			{				
				struct stat s;
				string path = (P2PFileTransfer::DATA_FOLDER + '/' + this_filename);
				if (stat(path.c_str(), &s) == 0 && (s.st_mode & S_IFREG))
				{
					size_downloaded += s.st_size;
				}
			}
		}

		return to_string(size_downloaded) + " of " + to_string(file_item.size) + " bytes";
	}

	return "Waiting for seeders...";
}

bool P2PPeerNode::hasSocketByName(string name)
{
	vector<P2PSocket>::iterator iter;
	for (iter = socket_vector.begin(); iter != socket_vector.end(); ++iter)
	{
		if ((*iter).name == name)
		{
			return true;
		}
	}

	return false;
}

P2PSocket P2PPeerNode::getSocketByName(string name)
{
	P2PSocket socket;
	vector<P2PSocket>::iterator iter;
	for (iter = socket_vector.begin(); iter != socket_vector.end(); ++iter)
	{
		if ((*iter).name == name)
		{
			socket = (*iter);
		}
	}

	return socket;
}

void P2PPeerNode::sendMessageToSocketName(string socket_name, string message)
{
	// Validate
	if (!hasSocketByName(socket_name))
	{
		return;
	}

	// Get a socket by name
	P2PSocket socket = getSocketByName(socket_name);
	if (write(socket.socket_id, message.c_str(), message.length()) < 0)
	{
		perror("Error: could not send message to server");
		exit(1);
	}
}

void P2PPeerNode::prepareFileTransferRequest(vector<string> request)
{
	prepareFileTransferRequest(request, 0, 0);
}

void P2PPeerNode::prepareFileTransferRequest(vector<string> request, int start, int count)
{
	// Trim any whitespace
	string file_id = P2PCommon::trimWhitespace(request[1]);

	// If the data came back invalid (possible race condition), just bail
	if (file_id == "NULL")
	{
		return;
	}

	// Convert the remaining data
	string name = P2PCommon::trimWhitespace(request[2]);
	int size = stoi(P2PCommon::trimWhitespace(request[3]));
	string address_pair = P2PCommon::trimWhitespace(request[4]);

	// Push to our local cache, only if it's not already there
	if (!hasDownloadFileItem(name, size))
	{
		// Keep a record of this file
		FileItem file_item;
		file_item.name = name;
		file_item.size = size;
		file_item.file_id = stoi(file_id);

		download_file_list.push_back(file_item);
	}
	else
	{
		// Get the start and count from the missing pieces
		FileItem file_item = getDownloadFileItem(name, size);
		start = file_item.missing_pieces.front();
		file_item.missing_pieces.erase(file_item.missing_pieces.begin());
		count = file_item.missing_pieces.front();
		file_item.missing_pieces.erase(file_item.missing_pieces.begin());
	}

	// Estimate the number of chunks that will be needed
	unsigned int num_chunks;
	if (size % P2PFileTransfer::FILE_CHUNK_SIZE == 0)
		num_chunks = size/P2PFileTransfer::FILE_CHUNK_SIZE;
	else
		num_chunks = size/P2PFileTransfer::FILE_CHUNK_SIZE + 1;

	// Dole out the requests to each available address
	int num_addresses = request.size() - 4;
	int i = 0;

	cout << "Found " << num_addresses << " peers holding this file." << endl;
	// Ensure that start is 1, minimum
	if (start <= 0)
	{
		start = 1;
	}

	// Ensure that count has a valid value, and divide the count among the available peers
	if (count <= 0)
	{
		count = num_chunks / num_addresses;
	}
	else if (count > 0)
	{
		count /= num_addresses;
	}

	while (i < num_addresses)
	{
		// Get the address pair
		address_pair = P2PCommon::trimWhitespace(request[4+i]);

		// Parse the address
		vector<string> address = P2PCommon::parseAddress(address_pair);

		// Make the connection
		makeConnection(file_id, address[0], stoi(address[1]));

		// Determine where we start and how many we dole out
		start += i * count;

		// Send the file request
		requestFileTransfer(file_id, stoi(file_id), name, size, start, count);
	
		// Move to the next address
		i++;
	}
}

void P2PPeerNode::requestFileTransfer(string socket_name, int file_id, string name, int size, unsigned int start, unsigned int count)
{
	// Compose the file transfer request
	string file_transfer_request = "fileRequest\r\n" + to_string(file_id)
		+ "\r\n" + name + "\r\n" + to_string(size)
		+ "\r\n" + to_string(start) + "\r\n" + to_string(count);
	sendMessageToSocketName(socket_name, file_transfer_request);
}

void * P2PPeerNode::initiateFileTransfer(void * arg)
{
	// Revive the packet
	FileDataRequest * request = static_cast<FileDataRequest *>(arg);

	P2PFileTransfer file_transfer;
	file_transfer.setBounds(request->start, request->count);
	file_transfer.startTransferFile(request->file_item, request->socket_id);

	// Finished - close the thread
	pthread_exit(NULL);
}

void * P2PPeerNode::handleFileTransfer(void * arg)
{
	// Revive the packet
	FileDataPacket * packet;
	packet = (FileDataPacket *) arg;

	P2PFileTransfer file_transfer;
	file_transfer.handleIncomingFileTransfer(*packet);

	//delete[] (*packet).packet;
	pthread_exit(NULL);
}

void P2PPeerNode::addDownloadFileItems(vector<FileItem> files)
{
	addFileItems(download_file_list, files);
}

bool P2PPeerNode::hasDownloadFileItem(string name, int size)
{
	return hasFileItem(download_file_list, name, size);
}

FileItem P2PPeerNode::getDownloadFileItem(string name, int size)
{
	return getFileItem(download_file_list, name, size);
}

FileItem P2PPeerNode::getDownloadFileItem(int file_id)
{
	return getFileItem(download_file_list, file_id);
}

void P2PPeerNode::addLocalFileItems(vector<FileItem> files)
{
	addFileItems(local_file_list, files);
}

bool P2PPeerNode::hasLocalFileItem(string name, int size)
{
	return hasFileItem(local_file_list, name, size);
}

FileItem P2PPeerNode::getLocalFileItem(string name, int size)
{
	return getFileItem(local_file_list, name, size);
}

FileItem P2PPeerNode::getLocalFileItem(int file_id)
{
	return getFileItem(local_file_list, file_id);
}

void P2PPeerNode::addFileItems(vector<FileItem> &existing_files, vector<FileItem> files_to_add)
{
	vector<FileItem>::iterator iter;
	for (iter = files_to_add.begin(); iter < files_to_add.end(); iter++)
	{
		if (!hasFileItem(existing_files, (*iter).name, (*iter).size))
		{
			existing_files.push_back((*iter));
		}
	}
}

bool P2PPeerNode::hasFileItem(vector<FileItem> &existing_files, string name, int size)
{
	// A map would be a better way to store and retrieve this information..
	vector<FileItem>::iterator iter;
	for (iter = existing_files.begin(); iter < existing_files.end(); iter++)
	{
		if ((*iter).name == name && (*iter).size == size)
		{
			return true;
		}
	}

	return false;
}

FileItem P2PPeerNode::getFileItem(vector<FileItem> &existing_files, string name, int size)
{
	// A map would be a better way to store and retrieve this information..
	FileItem file_item;
	vector<FileItem>::iterator iter;
	for (iter = existing_files.begin(); iter < existing_files.end(); iter++)
	{
		if ((*iter).name == name && (*iter).size == size)
		{
			file_item = (*iter);
			break;
		}
	}

	return file_item;
}

FileItem P2PPeerNode::getFileItem(vector<FileItem> &existing_files, int file_id)
{
	// A map would be a better way to store and retrieve this information..
	FileItem file_item;
	vector<FileItem>::iterator iter;
	for (iter = existing_files.begin(); iter < existing_files.end(); iter++)
	{
		if ((*iter).file_id == file_id)
		{
			file_item = (*iter);
			break;
		}
	}

	return file_item;
}
