#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#pragma warning(disable: 4996) 
using namespace std;
#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <string.h>
#include <time.h>
#include <ctime>
#include <chrono>
#include <sstream>
#include <string.h>

enum ClientRequest
{
	GET,
	HEAD,
	PUT,
	TRACE,
	DELETE1,
	OPTIONS,
	NF404,
};

struct SocketState
{
	SOCKET id;		
	int	recv;			
	int	send;			
	ClientRequest sendSubType;
	char buffer[4096];
	int len;
	time_t time;
};


const int TIME_PORT = 8080;
const int MAX_SOCKETS = 60;
const int EMPTY = 0;
const int LISTEN = 1;
const int RECEIVE = 2;
const int IDLE = 3;
const int SEND = 4;
const int SEND_TIME = 1;
const int SEND_SECONDS = 2;



bool addSocket(SOCKET id, int what);
void removeSocket(int index);
void acceptConnection(int index);
void receiveMessage(int index);
void sendMessage(int index);

struct SocketState sockets[MAX_SOCKETS] = { 0 };
int socketsCount = 0;

void main()
{
	time_t end;
	WSAData wsaData;
	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "Time Server: Error at WSAStartup()\n";
		return;
	}
	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == listenSocket)
	{
		cout << "Time Server: Error at socket(): " << WSAGetLastError() << endl;
		WSACleanup();
		return;
	}
	sockaddr_in serverService;
	serverService.sin_family = AF_INET;
	serverService.sin_addr.s_addr = INADDR_ANY;
	serverService.sin_port = htons(TIME_PORT);


	if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR *)&serverService, sizeof(serverService)))
	{
		cout << "Time Server: Error at bind(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	if (SOCKET_ERROR == listen(listenSocket, 5))
	{
		cout << "Time Server: Error at listen(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}
	addSocket(listenSocket, LISTEN);

	while (true)
	{
		fd_set waitRecv;
		FD_ZERO(&waitRecv);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if ((sockets[i].recv == LISTEN) || (sockets[i].recv == RECEIVE))
				FD_SET(sockets[i].id, &waitRecv);
		}

		fd_set waitSend;
		FD_ZERO(&waitSend);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if (sockets[i].send == SEND)
				FD_SET(sockets[i].id, &waitSend);
		}
		int nfd;
		nfd = select(0, &waitRecv, &waitSend, NULL, NULL);
		if (nfd == SOCKET_ERROR)
		{
			cout << "Time Server: Error at select(): " << WSAGetLastError() << endl;
			WSACleanup();
			system("pause");
			return;
		}

		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			int twoMinutes = 120;
			if (sockets[i].send == IDLE && sockets[i].recv != LISTEN)
			{
				time(&end);
				int secs = end - sockets[i].time;
				if (secs > twoMinutes)
				{
					cout << "socket " << sockets[i].id << "removed\n";
 					closesocket(sockets[i].id);
					removeSocket(i);
				}
			}
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitRecv))
			{
				nfd--;
				switch (sockets[i].recv)
				{
				case LISTEN:
					acceptConnection(i);
					break;

				case RECEIVE:
					receiveMessage(i);
					break;
				}
			}
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitSend))
			{
				nfd--;
				switch (sockets[i].send)
				{
				case SEND:
					sendMessage(i);
					break;
				}
			}
		}
	}

	cout << "Time Server: Closing Connection.\n";
	closesocket(listenSocket);
	WSACleanup();
}

bool addSocket(SOCKET id, int what)
{
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets[i].recv == EMPTY)
		{
			sockets[i].id = id;
			sockets[i].recv = what;
			sockets[i].send = IDLE;
			sockets[i].len = 0;
			time(&sockets[i].time);
			socketsCount++;
			return (true);
		}
	}
	return (false);
}

void removeSocket(int index)
{
	sockets[index].recv = EMPTY;
	sockets[index].send = EMPTY;
	sockets[index].time = 0;
	socketsCount--;
}

void acceptConnection(int index)
{
	SOCKET id = sockets[index].id;
	struct sockaddr_in from;	
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr *)&from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "Time Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "Time Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

	unsigned long flag = 1;
	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
	{
		cout << "Time Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}

	if (addSocket(msgSocket, RECEIVE) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}
	return;
}


bool isFileExists(const char* name) {
	if (FILE *file = fopen(name, "r")) {
		fclose(file);
		return true;
	}
	else {
		return false;
	}
}


void receiveMessage(int index)
{
	SOCKET msgSocket = sockets[index].id;
	int len = sockets[index].len;
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);

	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "Time Server: Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	if (bytesRecv == 0)
	{
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	else
	{
		sockets[index].buffer[len + bytesRecv] = '\0';

		sockets[index].len += bytesRecv;
		int sizeDecrese;
		if (sockets[index].len > 0)
		{
			if (strncmp(sockets[index].buffer, "GET", 3) == 0)
			{
				sizeDecrese = strlen("GET /");
				sockets[index].send = SEND;
				sockets[index].sendSubType = GET;
				memcpy(sockets[index].buffer, &sockets[index].buffer[sizeDecrese], sockets[index].len);
				sockets[index].len -= sizeDecrese;
				return;
			}
			else if (strncmp(sockets[index].buffer, "PUT", 3) == 0)
			{
				sizeDecrese = strlen("PUT /");
				sockets[index].send = SEND;
				sockets[index].sendSubType = PUT;
				memcpy(sockets[index].buffer, &sockets[index].buffer[sizeDecrese], sockets[index].len);
				sockets[index].len -= sizeDecrese;
				return;
			}
			else if (strncmp(sockets[index].buffer, "HEAD", 4) == 0)
			{
				sizeDecrese = strlen("HEAD /");
				sockets[index].send = SEND;
				sockets[index].sendSubType = HEAD;
				memcpy(sockets[index].buffer, &sockets[index].buffer[sizeDecrese], sockets[index].len);
				sockets[index].len -= sizeDecrese;
				return;
			}
			else if (strncmp(sockets[index].buffer, "DELETE", 6) == 0)
			{
				sizeDecrese = strlen("DELETE /");
				sockets[index].send = SEND;
				sockets[index].sendSubType = DELETE1;
				memcpy(sockets[index].buffer, &sockets[index].buffer[sizeDecrese], sockets[index].len);
				sockets[index].len -= sizeDecrese;
				return;
			}
			else if (strncmp(sockets[index].buffer, "TRACE", 5) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = TRACE;
				return;
			}
			else if (strncmp(sockets[index].buffer, "OPTIONS", 7) == 0)
			{
				sizeDecrese = strlen("OPTIONS /");
				sockets[index].send = SEND;
				sockets[index].sendSubType = OPTIONS;
				memcpy(sockets[index].buffer, &sockets[index].buffer[sizeDecrese], sockets[index].len);
				sockets[index].len -= sizeDecrese;
				return;
			}
			else
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = NF404;
			}
		}
	}
}

string ReadFromFile(string file_name)
{
	ifstream in_file;
	stringstream strStream;
	in_file.open(file_name, ios::in);
	strStream << in_file.rdbuf();
	in_file.close();
	return strStream.str();
}

string AddHeaders(string userContent)
{
	stringstream headers;
	time_t timer;
	time(&timer);
	char* time = ctime(&timer);
		headers << "Date: " << time
		<< "Server: Daniel&Gal server\r\n"
		<< "Content-Length: " << userContent.length() << "\r\n"
		<< "Content-Type: text/html\r\n" << "\r\n"
		<< userContent;

	return headers.str();
}

string AddRespose(string userSendBuff , string response)
{
	stringstream headers;
	headers << response << endl << userSendBuff;

	return headers.str();
}

void writeToGivenFile(string fileName, string content)
{
	std::ofstream file(fileName);
	file << content;
	file.close();
}

string AddContent(string buffer, string content)
{
	content = "/r/n/r/n" + content + "/r/n/r/n";
	buffer += content;
	return buffer;
}

void sendMessage(int index)
{
	int bytesSent = 0;

	string Code200 = "HTTP/1.1 200 OK";
	string Code201 = "HTTP/1.1 201 Created";
	string Code202 = "HTTP/1.1 202 Accepted";
	string Code204 = "HTTP/1.1 204 No Content";
	string Code400 = "HTTP/1.1 400 Bad Request";
	string Code404 = "HTTP/1.1 404 Not Found";


	string buffer = "";
	SOCKET msgSocket = sockets[index].id;
	if (sockets[index].sendSubType == GET)
	{
		string requstedFile = sockets[index].buffer;
		requstedFile = requstedFile.substr(0, requstedFile.find(" "));
		if (isFileExists(requstedFile.c_str()))
		{
			buffer = ReadFromFile(requstedFile);
			buffer = AddHeaders(buffer);
			buffer = AddRespose(buffer, Code200);
		}
		else
		{
			buffer = AddHeaders(buffer);
			buffer = AddRespose(buffer, Code404);
		}
	}
	else if (sockets[index].sendSubType == PUT)
	{
		string requstedFile = sockets[index].buffer;
		requstedFile = requstedFile.substr(0, requstedFile.find(" "));
		string content = sockets[index].buffer;
		content = content.substr(content.find("\r\n\r\n") + strlen("\r\n\r\n"), content.find_last_of("\r\n\r\n") + 4);
		if (isFileExists(requstedFile.c_str()))
		{
			writeToGivenFile(requstedFile, content);
			buffer = AddHeaders(buffer);
			buffer = AddRespose(buffer, Code202);
		}
		else
		{
			writeToGivenFile(requstedFile, content);
			buffer = AddHeaders(buffer);
			buffer = AddRespose(buffer, Code201);
		}
	}
	else if (sockets[index].sendSubType == DELETE1)
	{
		string requstedFile = sockets[index].buffer;
		requstedFile = requstedFile.substr(0, requstedFile.find(" "));
		if (isFileExists(requstedFile.c_str()))
		{
			remove(requstedFile.c_str());
			buffer = AddHeaders(buffer);
			buffer = AddRespose(buffer, Code200);
		}
		else
		{
			buffer = AddHeaders(buffer);
			buffer = AddRespose(buffer, Code404);
		}
	}
	else if (sockets[index].sendSubType == HEAD)
	{
		buffer = AddHeaders(buffer);
		buffer = AddRespose(buffer, Code200);
	}
	else if (sockets[index].sendSubType == TRACE)
	{
		string content = sockets[index].buffer;
		buffer = AddHeaders(buffer);
		buffer = AddRespose(buffer, Code200);
		buffer = AddContent(buffer, content);
	}
	else if (sockets[index].sendSubType == OPTIONS)
	{
		string Allow = Code200 + "\r\nAllow: GET, HEAD, PUT, TRACE, DELETE, OPTIONS";
		buffer = AddHeaders(buffer);
		buffer = AddRespose(buffer, Allow);
	}
	else if (sockets[index].sendSubType == NF404)
	{
		buffer = AddHeaders(buffer);
		buffer = AddRespose(buffer, Code404);
	}


	bytesSent = send(msgSocket, buffer.c_str(), buffer.length(), 0);
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "Time Server: Error at send(): " << WSAGetLastError() << endl;
		return;
	}
	sockets[index].buffer[0] = '\0';
	sockets[index].len = 0;
	cout << "Time Server: Sent: " << bytesSent << "\\" << buffer.length() << " bytes of \n" << "\"" << buffer << "\"" << "message.\n";

	sockets[index].send = IDLE;
	time(&sockets[index].time);
}

