#ifndef INCLUDE_HPP
#define INCLUDE_HPP

#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
// inet_ntop
#include <arpa/inet.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#define MAXLINE 4096

#define C_RES "\033[0m"
#define C_RED "\033[1;31m"
#define C_GREEN "\033[1;32m"
#define C_CYAN "\033[1;36m"
#define C_YELLOW "\033[1;33m"

#define print_line(str) cout << C_CYAN << "--------------------------------" << str << C_RES << endl;
#define print_error(str) cerr << C_RED << "--------------------------------" << str << C_RES << endl;

#define str_red(str) (std::string(C_RED) + std::string(str) + std::string(C_RES))
#define str_green(str) (std::string(C_GREEN) + std::string(str) + std::string(C_RES))
#define str_cyan(str) (std::string(C_CYAN) + std::string(str) + std::string(C_RES))

typedef struct sockaddr_in t_sockaddr_in;
typedef struct sockaddr	   t_sockaddr;

using std::cerr;
using std::cout;
using std::endl;
using std::map;
using std::string;
using std::vector;

#endif // INCLUDE_HPP