/**
 * Peer-to-peer file transfer class
 */

#include "P2PFileTransfer.hpp"

/**
 * Public Methods
 */
const string P2PFileTransfer::DATA_FOLDER = "P2PSharedFile";

P2PFileTransfer::P2PFileTransfer() {}

void P2PFileTransfer::setBounds(unsigned int start, unsigned int count)
{
	this->start = start;
	this->count = count;
}

void P2PFileTransfer::startTransferFile(FileItem file_item, int socket_id)
{
	// Get the file ID and path
	int file_id = file_item.file_id;
	string path = file_item.path;

	// Get the filename
	string filename = file_item.name;

	// If the filename is greater than 255 characters, trim it accordingly
	if (filename.length() > P2PCommon::MAX_FILENAME_LENGTH)
	{
		vector<string> filename_info = P2PCommon::splitString(filename, '.');
		string filename_ext = filename_info.back();

		// Return a filename that fits
		filename = filename.substr(0, P2PCommon::MAX_FILENAME_LENGTH - 1 - filename_ext.length()) + '.' + filename_ext;
	}

	// Load in the file bit by bit
	ifstream input_stream(path, ios::binary);
	if (input_stream)
	{
		// Get length of file
		input_stream.seekg(0, input_stream.end);
		unsigned int length = input_stream.tellg();

		// Determine number of file chunks
		unsigned int num_chunks;
		if (length % FILE_CHUNK_SIZE == 0)
			num_chunks = length/FILE_CHUNK_SIZE;
		else
			num_chunks = length/FILE_CHUNK_SIZE + 1;

		// Start back at beginning
		input_stream.seekg(0, input_stream.beg);

		// Allocate memory for sending data in chunks
		char * buffer = new char[FILE_CHUNK_SIZE + HEADER_SIZE];

		// Read data as blocks
		unsigned int i = 0;
		unsigned int total = num_chunks;

		if (start > 0 && start <= num_chunks)
			i = start;

		if (count > 0 && (count+i) <= num_chunks)
			total = (count+i);

		// Start at the proper location
		if (i > 0)
			input_stream.seekg((i-1) * FILE_CHUNK_SIZE);
	
		while (i <= total)
		{
			// Clear out the buffer
			memset(buffer, 0, FILE_CHUNK_SIZE + HEADER_SIZE);

			/*
				Header:
					flag (12) + 2 = 14 chars
					file id = 10 + 1 = 11 chars
					payload size = 5 + 1 = 6 chars
					total number of chunks (10) + 1, current chunk (10) + 1 = 22 chars
					checksum (8) + 2 = 10 chars
					payload (variable)
			// \t\r\n + file id + paysize + flag + chunks  + checksum = header
			// 8      + 10      + 5       + 12   + 10 + 10 + 8        = 63
			*/

			// Read in the data to send
			input_stream.read(&buffer[HEADER_SIZE], FILE_CHUNK_SIZE);
			int bytes_read = input_stream.gcount();

			// Compute the checksum
			string checksum = computeChecksum(&buffer[HEADER_SIZE], bytes_read);

			// Prepend the header
			char header[HEADER_SIZE+1]; // +1 = null terminator
			sprintf(header, "%12s\r\n%10d\t%5d\t%10d\t%10d\t%8s\r\n", 
				"fileTransfer", file_id, bytes_read, num_chunks, i, checksum.c_str());

			// Copy the header to the buffer
			memcpy(&buffer[0], header, HEADER_SIZE);

			// Send the data across the wire
			int bytes_written = write(socket_id, buffer, HEADER_SIZE + bytes_read);
			if (bytes_written == -1)
			{
				perror("Error: could not write to socket");
			}

			if (bytes_written < HEADER_SIZE + bytes_read)
			{
				// TO BE HANDLED - when write could not deliver the full payload
			}

			// If we write too fast, then the messages are sent in one packet
			usleep(5000);

			// Move to the next chunk
			input_stream.seekg((i++) * FILE_CHUNK_SIZE);
		}

		// Close and deallocate
		input_stream.close();
		delete[] buffer;
	}
	else
	{
		perror("Error: could not initiate file transfer");
	}
}

void P2PFileTransfer::handleIncomingFileTransfer(FileDataPacket packet)
{
	// Get the raw message and file information from the packet
	FileItem file_item = packet.file_item;
	char * raw_message = packet.packet;

	// Parse the request
	vector<string> request = P2PCommon::parseRequest(raw_message);
	vector<string> header_info = P2PCommon::splitString(request[1], '\t');

	// Direct the data stream into the correct file
	string file_id = P2PCommon::trimWhitespace(header_info[0]);
	int payload_size = stoi(P2PCommon::trimWhitespace(header_info[1]));
	string total_file_parts = P2PCommon::trimWhitespace(header_info[2]);
	string file_part = P2PCommon::trimWhitespace(header_info[3]);
	string checksum = P2PCommon::trimWhitespace(header_info[4]);

	// Validate the checksum before we do anything
	string computed_checksum = computeChecksum(&raw_message[HEADER_SIZE], payload_size);

	if (checksum.compare(computed_checksum) != 0)
	{
		perror("Error: calculated checksums do not match.");
		return;
	}

	// Ensure we have the data storage folder to work with
	struct stat s;
	int file_status = stat(P2PFileTransfer::DATA_FOLDER.c_str(), &s);
	if (!(file_status == 0 && (s.st_mode & S_IFDIR)))
	{
		if (mkdir(P2PFileTransfer::DATA_FOLDER.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
		{
			string message = "Error: could not create folder to save data. Create folder called " + P2PFileTransfer::DATA_FOLDER;
			perror(message.c_str());
			return;
		}
	}

	// Construct the download filename
	string filename = file_id + ".pt." + file_part + ".of." + total_file_parts + ".p2pft";

	// Save the piece to this folder
	string data_filename = P2PFileTransfer::DATA_FOLDER + "/" + filename;
	std::ofstream outfile(data_filename.c_str(), ios::binary);
	if (outfile)
	{
		// Write and close
		outfile.write(&raw_message[HEADER_SIZE], payload_size);
		outfile.flush();
		outfile.close();
	}
	else
	{
		perror("Error: could not open file to write");
	}
}

bool P2PFileTransfer::compileFileParts(FileItem & file_item)
{
	// Keep track of whether the file is completed
	bool b_file_completed = false;

	// Compute the total number of parts from the file size
	unsigned int length = file_item.size;
	unsigned int num_chunks;
	if (length % FILE_CHUNK_SIZE == 0)
		num_chunks = length/FILE_CHUNK_SIZE;
	else
		num_chunks = length/FILE_CHUNK_SIZE + 1;

	// Once we've saved the data, analyze to see if we have a full file
	// Add all files in the directory
	DIR *directory;
	struct dirent *ep;

	// Open the directory
	directory = opendir(P2PFileTransfer::DATA_FOLDER.c_str());
	if (directory == NULL)
	{
		return false;
	}

	// Get the file information
	string file_id = to_string(file_item.file_id);
	string filename = file_item.name;

	// Prepare the array 
	bool pieces_found[num_chunks];
	for (unsigned int i = 1; i <= num_chunks; i++)
	{
		pieces_found[i] = false;
	}

	// See how many parts we have available
	unsigned int i = 0;
	while ((ep = readdir(directory)) != NULL && num_chunks >= i)
	{
		string this_filename = string(ep->d_name);

		// Validate the filename length
		if (this_filename.length() < 8)
		{
			continue;
		}

		// Evaluate that each part is there
		if (this_filename.compare(0, file_id.length() + 1, (file_id + ".").c_str()) == 0
			&& this_filename.compare(this_filename.length() - 6, 6, ".p2pft") == 0)
		{
			// Get the piece number
			vector<string> pieces = P2PCommon::splitString(this_filename, '.');

			// Keep track of found pieces so we can request the missing ones
			pieces_found[stoi(pieces[2])] = true;

			i++;
		}
	}

	// If we have all the parts, combine them
	if (i == num_chunks)
	{
		string final_filename = P2PFileTransfer::DATA_FOLDER + '/' 
			+ P2PCommon::renameDuplicateFile(filename, P2PFileTransfer::DATA_FOLDER);

		std::ofstream outfile(final_filename.c_str(), ios::binary);

		if (outfile)
		{
			for (i = 1; i <= num_chunks; i++)
			{
				// Get the current file to read
				string this_filename = P2PFileTransfer::DATA_FOLDER + '/' + file_id 
					+ ".pt." + to_string(i) + ".of." + to_string(num_chunks) + ".p2pft";

				// Open the file to read in
				ifstream input_stream(this_filename, ios::binary);
				if (input_stream)
				{
					// Get length of file
					input_stream.seekg(0, input_stream.end);
					unsigned int length = input_stream.tellg();
					input_stream.seekg(0, input_stream.beg);

					// Allocate memory for transferring data
					char * buffer = new char[length];
					input_stream.read(buffer, length);

					// Read the contents of this file into the general file
					outfile.write(buffer, length);
					outfile.flush();
				}

				// Delete the unneeded chunk
				if (remove(this_filename.c_str()) != 0)
				{
					perror("Error: Could not remove outdated file chunk");
				}
			}

			// Notify the user
			cout << "Your download of \"" << filename << "\" is completed." << endl;
			b_file_completed = true;
			file_item.path = final_filename;

			// Close the output file
			outfile.close();
		}
		else
		{
			perror("Error: could not open file to write");
		}
	}
	else
	{
		// If we're missing pieces, determine which ones we're missing
		int start = 0; int count = 0;
		for (int i = 1; i <= num_chunks; i++)
		{
			if (!pieces_found[i] && start < 1)
			{
				start = i;
				count = 1;
			}
			else if (!pieces_found[i] && start >= 1)
			{
				count++;
			}

			if ((pieces_found[i] && start >= 1) || (i == num_chunks && start >= 1))
			{
				// We have a range - save it so that the peernode can restart the transfer
				file_item.missing_pieces.push_back(start);
				file_item.missing_pieces.push_back(count);
				start = 0;
				count = 0;

			}
		}
	}

	// Close the directory
	closedir(directory);

	return b_file_completed;
}

string P2PFileTransfer::computeChecksum(char * data, int size)
{
	unsigned int checksum = 0;
	unsigned int value = 0;
	int i = 0;
	while (i < size)
	{
		// Add on the next value
		value <<= 8;
		value |= data[i];

		// If we have a full value, add it to the checksum
		if ((i+1)%4 == 0)
		{
			checksum += value;
			checksum += (checksum <= value) ? 1 : 0; // Overflow -> Wraparound, add 1 bit
			value = 0;
		}

		// Move to the next iteration
		i++;
	}

	// Handle leftover values
	if (value > 0)
	{
		checksum += value;
		checksum += (checksum <= value) ? 1 : 0; 
	}

	// Convert the checksum to hex and return as a string
	stringstream sstream;
	sstream << hex << checksum;
	return sstream.str();
}

void P2PFileTransfer::reviewTransfers(vector<FileItem> &download_file_list)
{
	bool b_file_completed = false;
	vector<FileItem>::iterator iter;
	for (iter = download_file_list.begin(); iter < download_file_list.end(); iter++)
	{
		// Skip finished files
		if ((*iter).completed)
		{
			continue;
		}

		// Compile the file's parts
		(*iter).completed = compileFileParts(*iter);
	}
}
