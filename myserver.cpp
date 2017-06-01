#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <netdb.h>
#include <netinet/in.h>
#include <time.h>
#include <string>
#include <signal.h>
#include <sys/ioctl.h>
#include <cstdio>

using namespace std;

int count = 0;
volatile bool is_sig = false;

string
get_cont_type(const string &s, uint level = 0)
{
	string ans;
	if (level >= s.size()) {
		throw "400 Bad Request";
	}
	if (level == 4) {
		if (s[s.size() - level - 1] == '.') {
			return ".";
		} else {
			throw "400 Bad Request";
		}
	} else if (s[s.size() - level - 1] == '.') {
		return ".";
	} else {
		ans = get_cont_type(s,level + 1) + s[s.size() - level - 1];
	}
	return ans;
}

void
check_uri(const string & s, string & content_type, bool is_cgi)
{
	for (uint i = 0; i < s.size(); i++)
	{
		if (s[i] == ' ') {
			throw "400 Bad Request";
		}
	}
	if (!is_cgi) {
		string type = get_cont_type(s);
		if (type == ".txt") {
			content_type = "text/plain";
		} else if (type == ".html") {
			content_type = "text/html";
		} else if (type == ".jpg" || type == ".jpeg") {
			content_type = "image/jpeg";
		} else {
			throw "400 Bad Request";
		}
	} else {
		if (s[s.size()-1] == '&') {
			throw "400 Bad Request";
		}
		content_type = "text/plain";
	}
}
		

string
int_to_str(unsigned int x)
{
	string ans;
	if (x < 10) {
		ans.push_back(x + '0');
	} else {
		ans = int_to_str(x / 10);
		ans.push_back(x % 10 + '0');
	}
	return ans;
}

string
cpp_str(const char *str, unsigned int len)
{
	string ans;
	for (uint i = 0; i < len && i < strlen(str); i++)
	{
		ans.push_back(str[i]);
	}
	return ans;
}



int
cpy(char *str, string &str1)
{
	for (uint i = 0; i < str1.size(); i++)
	{
		str[i] = str1[i];
	}
	return 0;
}

string
pathpars(string & path)
{
	unsigned int i;
	string path1;
	for (i = 0; i < path.size(); i++)
	{
		if (path[i] == '?') {
			break;
		}
		path1.push_back(path[i]);
	}
	string argument;
	for (i++; i < path.size(); i++)
	{
		argument.push_back(path[i]);
	}
	path = path1;
	return argument;
}

char **
envcreate(string & path)
{
	string str, argument = pathpars(path);
	char **env = new char*[10];
	str = "CONTENT_TYPE=text/plain";
	env[0] = new char[str.size() + 1];
	cpy(env[0],str);
	env[0][str.size()] = '\0';
	str = "GATEWAY_INTERFACE=CGI/1.1";
	env[1] = new char[str.size() + 1];
	cpy(env[1],str);
	env[1][str.size()] = '\0';
	str = "SERVER_ADDR=127.0.0.1";
	env[2] = new char[str.size() + 1];
	cpy(env[2],str);
	env[2][str.size()] = '\0';
	str = "SERVER_PORT=1234";
	env[3] = new char[str.size() + 1];
	cpy(env[3],str);
	env[3][str.size()] = '\0';
	str = "SERVER_PROTOCOL=HTTP/1.1";
	env[4] = new char[str.size() + 1];
	cpy(env[4],str);
	env[4][str.size()] = '\0';
	str = "SCRIPT_NAME=" + path;
	env[5] = new char[str.size() + 1];
	cpy(env[5],str);
	env[5][str.size()] = '\0';
	str = "SCRIPT_FILENAME=" + cpp_str(getenv("PWD"),strlen(getenv("PWD"))) + path;
	env[6] = new char[str.size() + 1];
	cpy(env[6],str);
	env[6][str.size()] = '\0';
	str = "DOCUMENT_ROOT=" + cpp_str(getenv("PWD"),strlen(getenv("PWD")));
	env[7] = new char[str.size() + 1];
	cpy(env[7],str);
	env[7][str.size()] = '\0';
	str = "QUERY_STRING=" + argument;
	env[8] = new char[str.size() + 1];
	cpy(env[8],str);
	env[8][str.size()] = '\0';
	env[9] = NULL;
	return env;
}

namespace cgi_space {
	char **env;
}

void
clear(void)
{
	for (int i = 0; i < 9; i++)
	{
		delete [] cgi_space::env[i];
	}
	delete [] cgi_space::env;
}
	

int
cgi(string & path)
{
	cgi_space::env = envcreate(path);
	string path1;
	int pid = fork(), fd;
	if (pid < 0) {
		throw "500 Internal Server Error";
	}
	if (pid == 0) {
		if ((fd = open((cpp_str(getenv("PWD"),strlen(getenv("PWD"))) + "/cgi-bin/tmp.txt").c_str(),
						O_WRONLY|O_CREAT|O_TRUNC),0777) < 0) {
			throw "500 Internal Server Error";
		}
		atexit(clear);
		dup2(fd,1);
		execvpe((cpp_str(getenv("PWD"),strlen(getenv("PWD"))) + path).c_str(),
					cgi_space::env,cgi_space::env);
		perror("execvpe");
		exit(1);
	}
	time_t start = time(NULL);
	int status = 0, stat = 0;
	while (time(NULL) - start < 5 && stat == 0) {
		stat = waitpid(pid,&status,WNOHANG);
	}
	if (stat == 0) {
		kill(pid,SIGINT);
		throw "500 Internal Server Error";
	}
	if (!WIFEXITED(status)) {
		throw "500 Internal Server Error";
	}
	if (WEXITSTATUS(status) != 0) {
		throw "500 Internal Server Error";
	}
	clear();
	return 0;
}	

int
otvet(int clientsocket, string status = "200 OK")
{
	cout << "Я родился!\n";
	string str,msg_type,ans,msg,path,error_body,content_length;
	string content_type,date;
	bool is_cgi = false;
	char *str1 = new char[1000];
	char ch;
	int fd;
	if (recv(clientsocket,str1,1000,0) == -1) {
		perror("recv");
		exit(0);
	}
	msg = cpp_str(str1,strlen(str1));
	unsigned int i = 0;
	while (msg[i] != ' ') {
		msg_type.push_back(msg[i]);
		i++;
	}
	i++;
	try {
		if (status != "200 OK") {
			throw status.c_str();
		}
		if (msg_type != "GET" && msg_type != "HEAD") {
			throw "501 Not Implemented";
		}
		while (msg[i] != ' ') {
			path.push_back(msg[i]);
			i++;
		}
		i = 1;
		while (i != path.size() && path[i] != '/') {
			str.push_back(path[i]);
			i++;
		}
		if (str == "cgi-bin") {
			is_cgi = true;
		}
		if (path == "/") {
			path = "/index.html";
		}
		check_uri(path,content_type,is_cgi);
		if (is_cgi) {
			cgi(path);
			path = path = cpp_str(getenv("PWD"),strlen(getenv("PWD"))) + "/cgi-bin/tmp.txt";
		} else {
			path = cpp_str(getenv("PWD"),strlen(getenv("PWD"))) + path;
		}
		if ((fd = open(path.c_str(),O_RDONLY|O_NONBLOCK)) != -1) {
			struct stat buf;
			if (stat(path.c_str(),&buf) == -1) {
				perror("stat");
			}
			char *buf1 = new char[30];
			time_t tm = buf.st_mtime;
			strftime(buf1, 30, "%a, %d %b %Y %H:%M:%S GMT", localtime(&tm));
			date = cpp_str(buf1,30);
			delete []buf1;
		} else {
			perror("open");
			switch (errno) {
				case EACCES:
					throw "403 Forbidden";
					break;
				case ENOENT:
					throw "404 Not Found";
					break;
				default:
					break;
			}
		}
	}
	catch (const char * s) {
		status = s;
		content_type = "text/html";
		error_body = "<h1>Error! " + status + "</h1>\n";
		if (status == "400 Bad Request") {
			error_body += "URI contains invalid characters or requests invalid content";
		} else if (status == "403 Forbidden") {
			error_body += "Permission denied";
		} else if (status == "404 Not Found") {
			error_body += "File doesn't exist";
		} else if (status == "501 Not Implemented") {
			error_body += "Unsupported request type";
		} else if (status == "500 Internal Server Error") {
			error_body += "Some problems occurred while processing the request";
		} else if (status == "503 Service Unavailable") {
			error_body += "Service is temporarily unable to process the request";
		}
	}
	ans = "HTTP/1.1 "; 
	ans += status + "\nAllow: GET,HEAD\n";
	ans += "Server: Model HTTP server(wowah)/0.1\n";
	time_t tme = time(NULL);
	strftime(str1, 30, "%a, %d %b %Y %H:%M:%S GMT", localtime(&tme));
	ans += "Date: " + cpp_str(str1,30) + "\n";
	ans += "Last-modified: " + date + "\n";
	ans += "Content-type: " + content_type + "\n";
	ans += "Content-length: ";
	send(clientsocket,ans.c_str(),strlen(ans.c_str()),0);
	if (status == "200 OK") {
		int size = 0;
		ioctl(fd,FIONREAD,&size);
		content_length = int_to_str(size);
		send(clientsocket,content_length.c_str(),strlen(content_length.c_str()),0);
		send(clientsocket,"\n\n",2,0);
		if (msg_type == "GET") {
			for (int i = 0; i < size; i++)
			{
				read(fd,&ch,1);
				send(clientsocket,&ch,1,0);
			}
		}
	} else {
		sprintf(str1,"%d",strlen(error_body.c_str()));
		send(clientsocket,str1,strlen(str1),0);
		send(clientsocket,"\n\n",2,0);
		send(clientsocket,error_body.c_str(),strlen(error_body.c_str()),0);
	}	
	close(fd);
	shutdown(clientsocket,2);
	close(clientsocket);
	delete []str1;
	cout << "Я прожил достойную жизнь!\n";
	return 0;
}

void
sig(int)
{
	is_sig = true;
}

int
main(int argc, char **argv)
{
	fd_set read_set;
	int max_d, opt = 1;
	int pid;
	int serversocket = socket(AF_INET,SOCK_STREAM,0), clientsocket;
	struct sockaddr_in serveraddress;
	signal(SIGINT,sig);
	if (setsockopt(serversocket,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt)) == -1) {
		perror("setsockopt");
		exit(1);
	}
	serveraddress.sin_family = AF_INET;
	serveraddress.sin_port = htons(1234);
	serveraddress.sin_addr.s_addr = INADDR_ANY;
	if (bind(serversocket,(sockaddr*)&serveraddress,sizeof(serveraddress) + 1) < 0) {
		perror("bind");
		return 1;
	}
	if (listen(serversocket,4) < 0) {
		perror("listen");
		return 1;
	}
	while (!is_sig) {
		cout << "Новое начало - новая жизнь\n";
		max_d = serversocket;
		FD_ZERO(&read_set);
		FD_SET(serversocket,&read_set);
		if (select(max_d + 1, &read_set,0,0,0) < 1) {
			perror("select");
			return 1;
		}
		clientsocket = accept(serversocket,NULL,NULL);
		if (clientsocket < 0) {
			perror("accept");
			return 1;
		}
		if ((pid = fork()) < 0) {
			otvet(clientsocket,"503 Service Unavailable");
		} else if (pid == 0) {
			if (is_sig) {
				otvet(clientsocket,"503 Service Unavailable");
			} else {
				otvet(clientsocket);
			}
			exit(0);
		}
		cout << "Конец ожидает нас каждого\n\n";	
	}
	cout << "Конец?\n";
	close(serversocket);
	return 0;
}

