#include "MainClient.hpp"
// Getters
const map<string, string> &MainClient::get_request() const { return request_parser->get_request(); }

const string &MainClient::get_request(string key) { return request_parser->get_request(key); }

const bool &MainClient::get_send_receive_status() const { return send_receive_status; }

const int &MainClient::get_phase() const { return phase; }

const string &MainClient::get_body_file() const { return body_file; }

const int &MainClient::get_client_socket() const { return (client_socket); }

const int &MainClient::get_location() const { return (location); }

ConfigServerParser *MainClient::get_config_server() const { return (config_server_parser); }

// Setters
void MainClient::set_send_receive_status(bool send_receive_status) {
	this->send_receive_status = send_receive_status;
}

void MainClient::set_location(int location) { this->location = location; }

// Constructors and destructor
MainClient::MainClient() { std::memset(buffer, 0, MAXLINE + 1); }

MainClient::MainClient(int client_socket, ConfigServerParser *config_server_parser)
	: config_server_parser(config_server_parser), request_parser(new RequestParser()),
	  send_receive_status(true), msg_status(Accurate::OK200().what()), client_socket(client_socket),
	  status(200), phase(READ_PHASE), head_status(false), body_status(false) {
	std::memset(buffer, 0, MAXLINE + 1);
}

MainClient::~MainClient() { delete request_parser; }

// Methods
void MainClient::start(string task) {
	if (task == "read" || task == "write")
		this->start_handle(task);
	else
		throw std::runtime_error("Unknown task");
}

void MainClient::start_handle(string task) {
	try {
		if (task == "read") {
			this->handle_read();
			this->phase = WRITE_PHASE;
		}

		else if (task == "write")
			this->handle_write();

	} catch (const std::exception &e) {

		if (string(e.what()).find("can't open file") != string::npos)
			throw std::runtime_error(string(e.what()));

		print_error(string(e.what()));

		if (string(e.what()) == "Still running")
			return;

		print_short_line("catch something");
		set_header_for_errors_and_redirection(e.what());
		this->send_receive_status = false;
	}
	send_to_socket();
	if (task == "write")
		this->send_receive_status = false;
}

void MainClient::header_reading() {
	int bytes;

	if (this->head_status)
		return;

	std::memset(buffer, 0, MAXLINE);
	bytes = recv(this->client_socket, buffer, MAXLINE, 0);
	if (bytes == 0)
		return;
	if (bytes < 0)
		throw Error::BadRequest400();
	this->head.append(buffer, bytes);
	if (this->head.find("\r\n\r\n") != string::npos) {
		this->body		  = this->head.substr(this->head.find("\r\n\r\n") + 4);
		this->head		  = this->head.substr(0, this->head.find("\r\n\r\n") + 4);
		this->head_status = true;
		return;
	} else
		throw std::runtime_error("Still running");
}

string MainClient::generate_random_file_name() {
	std::stringstream ss;
	std::time_t		  now = std::time(0);

	// Seed the random number generator
	std::srand(static_cast<unsigned int>(std::time(0)));

	ss << "./tmp/body_" << std::hex << now << "_" << std::rand();
	return ss.str();
}

void MainClient::body_reading() {
	int		   n, bytes;
	static int count = 0;

	if (this->body_status)
		return;

	if (this->body_file.size() == 0) {
		this->body_file = generate_random_file_name();
		cout << "body file : " << this->body_file << endl;
	}

	// Open the file for writing
	std::ofstream outFile(this->body_file.c_str(), std::ios::app | std::ios::binary);
	if (!outFile)
		throw std::runtime_error(str_red("can't open file " + this->body_file));

	if (this->body.size() != 0) {
		count = this->body.size();
		outFile.write(this->body.c_str(), count);
		this->body.clear();
	}

	n = ConfigServerParser::stringToInt(this->request_parser->get_request("Content-Length"));
	if (n == 0 || n == count) {
		this->body_status = true;
		count			  = 0;
		return;
	}

	std::memset(buffer, 0, MAXLINE);
	bytes = recv(this->client_socket, buffer, MAXLINE, 0);
	if (bytes < 0)
		throw Error::BadRequest400();

	// Write data to the file
	outFile << buffer;
	count += bytes;

	// Close the file
	outFile.close();
	if (count == n || bytes == 0) {
		this->body_status = true;
		count			  = 0;
		return;
	} else {
		throw std::runtime_error("Still running");
	}
}

int MainClient::find_chunk_size0() {
	int			i	= 0;
	std::size_t pos = this->body.find("\r\n");
	if (pos != std::string::npos) {
		std::string		  chunkSizeStr = this->body.substr(0, pos);
		std::stringstream ss(chunkSizeStr);
		ss >> std::hex >> i;
	}
	return i;
}

int MainClient::find_chunk_size1() {
	int				  bytes;
	std::stringstream ss;
	string			  tmp_body;

	std::memset(buffer, 0, MAXLINE);
	for (int i = 0; i < MAXLINE; i++) {
		bytes = recv(this->client_socket, buffer, 1, 0);
		if (bytes == 0)
			return 0;
		if (bytes < 0)
			throw Error::BadRequest400();
		tmp_body.append(buffer, bytes);

		std::size_t pos = tmp_body.find("\r\n");
		if (pos != std::string::npos) {
			std::string chunkSizeStr = tmp_body.substr(0, pos);
			if (chunkSizeStr == "0" || chunkSizeStr.empty())
				return 0;
			std::stringstream ss(chunkSizeStr);
			return i;
		}
	}
	return 0;
}

void MainClient::chunked_body_reading() {
	int		   bytes;
	static int count = 0, n = 0;

	if (this->body_status)
		return;

	if (this->body_file.empty()) {
		this->body_file = generate_random_file_name();
		cout << "body file: " << this->body_file << endl;
	}

	// Open the file for writing
	std::ofstream outFile(this->body_file.c_str(), std::ios::app | std::ios::binary);
	if (!outFile)
		throw std::runtime_error(str_red("can't open file " + this->body_file));

	if (!this->body.empty()) {
		n = find_chunk_size0();

		std::size_t pos = this->body.find("\r\n");

		string tmp_body = this->body.substr(pos + 2);

		if (tmp_body.find("\r\n") != string::npos) {

			std::size_t pos2 = tmp_body.find("\r\n");

			string to_write = this->body.substr(pos + 2, pos2 + 2);

			count += to_write.size() - 2;

			// Write data to the file
			outFile.write(to_write.c_str(), count);

			this->body = tmp_body.substr(pos2 + 2);

		} else {
			string to_write = this->body.substr(pos + 2);

			count += to_write.size();

			// Write data to the file
			outFile.write(to_write.c_str(), count);

			this->body.clear();
		}
		n -= count;
	}

	if (count == 0 && n == 0)
		n = find_chunk_size1();
	if (n <= 0) {
		this->body_status = true;
		n				  = 0;
		outFile.close();  // Close the file
		return;
	}

	char chunked_buffer[n];
	std::memset(chunked_buffer, 0, n);
	bytes = recv(this->client_socket, chunked_buffer, n, 0);
	if (bytes < 0)
		throw Error::BadRequest400();

	// Write data to the file
	outFile.write(chunked_buffer, bytes);
	n -= bytes;

	outFile.close();  // Close the file

	count = 0;
	if (bytes == 0) {
		this->body_status = true;
		n				  = 0;
		return;
	} else {
		throw std::runtime_error("Still running");
	}
}

void MainClient::handle_read() {
	print_line("Client Request (read)");

	this->header_reading();
	this->request_parser->run_parse(this->head);

	if (this->request_parser->get_request("Request-Type") == "POST") {
		if (this->get_request("Content-Length").size() != 0)
			this->body_reading();
		else if (this->get_request("Transfer-Encoding") == "chunked")
			this->chunked_body_reading();
		else
			throw Error::BadRequest400();
	}
	
	int location = this->match_location();
	if (location != -1)
	{
		this->location = location;
		if (this->config_server_parser->get_config_location_parser()[get_location()]->get_return().size() != 0)
		{
			std::string root = this->config_server_parser->get_config_location_parser()[get_location()]->get_root();
			std::string ret = this->config_server_parser->get_config_location_parser()[get_location()]->get_return();
			redirection = root + '/' + ret;
			throw Accurate::MovedPermanently301();
		}
		is_method_allowed_in_location();
	}
}

void MainClient::handle_write() {
	print_line("Server Response (write)");

	Response Response(this);
	if (this->request_parser->get_request("Request-Type") == "GET") {
		serve_file = Response.Get(this);
	}
	if (this->request_parser->get_request("Request-Type") == "DELETE") {
		// DELETE
	}
}

void MainClient::is_method_allowed_in_location() {
	for (vector<ConfigLocationParser *>::const_iterator it
		 = config_server_parser->get_config_location_parser().begin();
		 it != config_server_parser->get_config_location_parser().end(); it++) {
		if (this->get_request("Request-URI").find((*it)->get_location()) != string::npos
			|| this->get_request("Request-URI").find((*it)->get_root()) != string::npos) {
			for (size_t i = 0; i < (*it)->get_methods().size(); i++) {
				if ((*it)->get_methods(i) == this->get_request("Request-Type"))
					return;
			}
		}
	}
	throw Error::MethodNotAllowed405();
}

int	MainClient::match_location()
{
	std::string str = this->get_request("Request-URI");
	size_t found;
	int locate = 0;
	this->new_url = this->get_request("Request-URI");
	while (str.size() != 0)
	{
		locate = 0;
		for (vector<ConfigLocationParser *>::const_iterator itr = config_server_parser->get_config_location_parser().begin();
		itr != config_server_parser->get_config_location_parser().end() ; itr++)
		{
			if ((*itr)->get_location() == str)
			{
				str = this->get_request("Request-URI");
				this->new_url = this->get_request("Request-URI");
				std::string root = this->config_server_parser->get_config_location_parser()[locate]->get_root();
				this->new_url.erase(0, (*itr)->get_location().size());
				this->new_url = root + new_url;// ? I shouldn't reset the uri for redirect it later
				return (locate);
			}
			locate++;
		}
		found = str.find_last_of('/');
		str = str.substr(0, found);
	}
	check_if_uri_exist();
	return (-1);
}

void MainClient::set_header_for_errors_and_redirection(const char *what)
{
	this->msg_status = what;
	this->status = convert_to_int(this->msg_status);
	if (this->status >= 400)
		check_files_error();
	if (this->status  < 400) // redirection
	{
		this->header = "HTTP/1.1 ";
		this->header += this->msg_status;
		this->header += "\r\nContent-Length: 0\r\n";
		this->header += "Location: "; //? should i use port and host or not 
		this->header += redirection;
		this->header += "\r\n\r\n";
	}
	else // errors
	{
		Response	Error;
		this->body_file = Error.SetError(msg_status, body_file);
		this->header = Error.GetHeader();
	}
	serve_file = body_file;
}

void MainClient::set_redirection(std::string &redirection)
{
	this->redirection = redirection;
}

std::string MainClient::get_new_url()
{
	return(this->new_url);
}

std::string	MainClient::get_serve_file()
{
	return(serve_file);
}

void	MainClient::check_if_uri_exist()
{
	DIR *directory = opendir(this->get_request("Request-URI").c_str());
	if (directory == NULL)
	{
		std::ifstream file(this->get_request("Request-URI"));
		if (!file.is_open())
			throw Error::NotFound404();
		file.close();
		this->serve_file = this->get_request("Request-URI");
		return;
	}
	else
	{
		if (this->get_request("Request-URI") == "/")
		{
			std::ofstream file("folder/root_directory.html");
			if (!file.is_open())
				throw Error::Forbidden403();
			char current_path[MAXLINE];
			getcwd(current_path, sizeof(current_path));
			DIR *root_directory = opendir(current_path);
			if (root_directory == NULL)
				throw Error::Forbidden403();
			this->serve_file = this->write_into_file(root_directory, current_path);
			closedir(root_directory);
			return;
		}
	}
	throw Error::NotFound404();
}

void	MainClient::check_files_error()
{
	std::map<int, std::string>error_map = this->config_server_parser->get_error_page();
	if (error_map[this->status].size() != 0)
	{
		std::ifstream error_page(error_map[this->status]);
		if (!error_page.is_open())
			throw Error::Forbidden403();
		body_file = error_map[this->status];
		error_page.close();
	}
}

std::string	MainClient::write_into_file(DIR *directory, std::string root)
{
	std::ofstream file("folder/serve_file.html");
	if (!file.is_open())
		throw Error::BadRequest400();
	file << "<!DOCTYPE html>\n<html>\n<head>\n<title>index of";
	file << root;
	file << "</title>\n<style>\nbody {\ntext-align: left;\npadding: 40px;\nfont-family: Arial, sans-serif;\n}\nh1 {\nfont-size: 32px;\ncolor: black;\n}\n</style>\n</head>\n<body>\n<h1>";
	file << "index of ";
	file << root;
	file << "</h1>\n";
	dirent *list;
	while ((list = readdir(directory)))
	{
		file << "<li> <a href= ";
		file << '"';
		file << list->d_name;
		file << '"';
		file << '>';
		file << list->d_name;
		file << "</a></li>";
	}
	file.close();
	return ("folder/serve_file.html");
}

int	MainClient::convert_to_int(std::string &str)
{
	int	integer;
	std::stringstream ss (this->msg_status);
	ss >> integer;
	return(integer);
}

void	MainClient::send_to_socket()
{
	std::cout << "this is first header to send: " << this->header << std::endl;
	if (this->status == 301)
		send(client_socket, this->header.c_str(), header.size(), 0);
	else
	{
		std::ifstream file(serve_file, std::ios::binary);
		if (!file.is_open())
			throw Error::Forbidden403();
		int chunk = 1024;
		long count = 0;
		send(client_socket, this->header.c_str(), header.size(), 0);
		while (!file.eof())
		{
			char buff[chunk];
			file.read(buff, chunk);
			count += file.gcount();
			// std::cout.write(buff, file.gcount());
			send(client_socket, buff, file.gcount(), 0);
		}
		file.close();
	}
}


void	MainClient::set_content_type_map()
{
	this->content_type[".txt"] = "text/plain";
	this->content_type[".csv"] = "text/plain";
	this->content_type[".html"] = "text/html";
	this->content_type[".htm"] = "text/plain";
	this->content_type[".css"] = "text/css";
	this->content_type[".jpeg"] = "image/jpeg";
	this->content_type[".jpg"] = "image/jpeg";
	this->content_type[".png"] = "image/png";
	this->content_type[".gif"] = "image/gif";
	this->content_type[".ico"] = "image/icon";
	this->content_type[".svg"] = "image/svg+xml";
	this->content_type[".mp3"] = "audio/mpeg";
	this->content_type[".wav"] = "audio/wav";
	this->content_type[".mp4"] = "video/mp4";
	this->content_type[".mov"] = "video/quicktime";
	this->content_type[".js"] = "application/javascript";
	this->content_type[".js"] = "application/json";
	this->content_type[".xml"] = "application/xml";
	this->content_type[".pdf"] = "application/pdf";
}


// import os
// file_path = "./error/404.html"
// file_size = os.path.getsize(file_path)
// print("File size:", file_size, "bytes")
//
