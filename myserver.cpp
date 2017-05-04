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


using namespace std;

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
check_uri(const string & s, string & content_type)
{
	for (uint i = 0; i < s.size(); i++)
	{
		if ((s[i] < 'a' || s[i] > 'z') && (s[i] < 'A' || s[i] > 'Z') && 
			(s[i] < '0' || s[i] > '9') && s[i] != '/' && s[i] != '.' && 
			s[i] != '?' && s[i] != '=' && s[i] != '&') {
			cout << s[i] << endl;
			throw "400 Bad Request";
		}
	}
	string type = get_cont_type(s);
	if (type == ".txt" || type == ".html") {
		content_type = "text/html";
	} else if (type == ".jpg" || type == ".jpeg") {
		content_type = "image/jpeg";
	} else {
		throw "400 Bad Request";
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

string
fileread(const string & pathname, string & date)
{
	string ans;
	char ch;
	ifstream fin;
	//int fd;
	fin.open(pathname.c_str(),ios_base::binary|ios_base::in);
	if (fin.is_open()) {
	//if ((fd = open(pathname.c_str(),O_RDONLY|O_NONBLOCK)) != -1) {
		while (!fin.eof()) {
		//while (read(fd,&ch,sizeof(char)) == 1) {
			fin.get(ch);
			if (!fin.eof()) {
				ans.push_back(ch);
			}
		}
		fin.close();
		struct stat buf;
		if (stat(pathname.c_str(),&buf) == -1) {
			perror("stat");
		}
		char *buf1 = new char[30];
		time_t tm = buf.st_mtime;
		strftime(buf1, 30, "%a, %d %b %Y %H:%M:%S GMT", localtime(&tm));
		date = cpp_str(buf1,30);
		delete []buf1;
		//close(fd);
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
	return ans;
}

int
otvet(int clientsocket)
{
	string s,msg_type,ans,msg,path,file_body,content_type,date,status = "200 OK";
	char *str1 = new char[1000];
	recv(clientsocket,str1,1000,0);
	msg = cpp_str(str1,strlen(str1));
	int i = 0;
	while (msg[i] != ' ') {
		msg_type.push_back(msg[i]);
		i++;
	}
	i++;
	try {
		if (msg_type != "GET" && msg_type != "HEAD") {
			throw "501 Not Implemented";
		}
		while (msg[i] != ' ') {
			path.push_back(msg[i]);
			i++;
		}
		if (path == "/") {
			path = "/index.html";
		}
		path = cpp_str(getenv("PWD"),strlen(getenv("PWD"))) + path;
		check_uri(path,content_type);
		file_body = fileread(path,date);
	}
	catch (const char *s) {
		status = s;
		file_body = "<h1>Error! " + status + "</h1>\n";
		if (status == "400 Bad Request") {
			file_body += "URI contains invalid characters or requests invalid content";
		} else if (status == "403 Forbidden") {
			file_body += "Permission denied";
		} else if (status == "404 Not Found") {
			file_body += "File doesn't exist";
		} else if (status == "501 Not Implemented") {
			file_body += "Unsupported request type";
		}
	}
	ans = "HTTP/1.1 "; 
	ans += status + "\nAllow: GET,HEAD\n";
	ans += "Server: Model HTTP server(wowah)/0.1\n";
	time_t tme = time(NULL);
	strftime(str1, 30, "%a, %d %b %Y %H:%M:%S GMT", localtime(&tme));
	ans += "Date: " + cpp_str(str1,30) + "\n";
	ans += "Last-modified: " + date + '\n';
	ans += "Content-type: " + content_type + '\n';
	ans += "Content-length: " + int_to_str(file_body.size()) + "\n\n";
	if (msg_type == "GET") {
		ans += file_body;
	}
	cout << file_body.size() << endl;
	send(clientsocket,ans.c_str(),strlen(ans.c_str()) + 1,0);
	cout << msg << endl;
	delete []str1;
	return 0;
}

int
main(int argc, char **argv)
{
	fd_set read_set;
	int max_d, opt = 1;
	int serversocket = socket(AF_INET,SOCK_STREAM,0);
	struct sockaddr_in serveraddress;
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
	if (listen(serversocket,1) < 0) {
		perror("listen");
		return 1;
	}
	while (1) {
		max_d = serversocket;
		FD_ZERO(&read_set);
		FD_SET(serversocket,&read_set);
		if (select(max_d + 1, &read_set,0,0,0) < 1) {
			perror("select");
		}
		int clientsocket = accept(serversocket,NULL,NULL);
		if (clientsocket < 0) {
			perror("accept");
			return 1;
		}
		otvet(clientsocket);
		shutdown(clientsocket,2);
		close(clientsocket);
		//break;	
	}
	close(serversocket);
	return 0;
}

