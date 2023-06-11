#ifndef MAINCLIENT_HPP
#define MAINCLIENT_HPP

#include "WSNetworking.hpp"

class MainClient {

  private:
  	std::map<std::string, std::string>content_type;
	ConfigServerParser *config_server_parser;
	RequestParser	   *request_parser;
	bool				send_receive_status;
	string				msg_status;
	int					client_socket;
	char				buffer[MAXLINE];
	string				header;
	int					location;
	std::string			redirection;
	std::string			new_url;
	std::string			serve_file;
	std::string			body_file;
	int					status, phase;

  private:
	// Copy constructor and assignation operator
	MainClient(const MainClient &);
	MainClient &operator=(const MainClient &);

	string head, body, body_file;
	bool   head_status, body_status;

  public:
	// Getters
	const map<string, string> &get_request() const;
	const string			  &get_request(string key);
	const bool				  &get_send_receive_status() const;
	const int				  &get_phase() const;
	const string			  &get_body_file() const;
	const int				  &get_client_socket() const;
	const int				  &get_location() const;
	ConfigServerParser		  *get_config_server() const;

	// Setters
	void set_send_receive_status(bool send_receive_status);
	void set_location(int location);

	// Constructors and destructor
	MainClient();
	MainClient(int client_socket, ConfigServerParser *config_server_parser);
	~MainClient();

	// Methods
	int		GetClientSocket();
	void	set_header_for_errors_and_redirection(const char *what);
	ConfigServerParser *get_config_server();
	void set_redirection(std::string &redirection);
	std::string get_new_url();
	std::string	get_serve_file();
	std::string	write_into_file(DIR *directory, std::string root);
	int	convert_to_int(std::string	&str);
	void	set_serve_file(std::string file_to_serve);
	void	set_header(std::string header);
	void	send_to_socket();
	void	set_content_type_map();

	void start_handle();
	void start(string task);

  private:
	// Methods
	void start_handle(string task);

	void header_reading();

	string generate_random_file_name();
	void   body_reading();

	int	 find_chunk_size0();
	int	 find_chunk_size1();
	void chunked_body_reading();

	void handle_read();
	void handle_write();

	int		match_location();
	void	is_method_allowed_in_location();
	void	check_if_uri_exist();
	void	check_files_error();
};

#endif	// MAINCLIENT_HPP