#include <iostream> 
#include <string> 
#include <vector> 
#include <WinSock2.h> 
#include <WS2tcpip.h> 
#include <Windows.h> 
#include <thread> 
#include <fstream>
#include <sstream>
#include <filesystem>

#pragma comment (lib, "Ws2_32.lib") //сообщает линкеру добавить Ws2_32.lib к проекту автоматически

using std::cout, std::endl, std::cerr;

void work_with_client(SOCKET cs, const short BUFF_SIZE);

std::string siteData(std::string path);

bool writeData(std::string data);

bool deleteFile(const std::string& filename);

std::string getTime()
{
	using namespace std;
	SYSTEMTIME st;
	GetLocalTime(&st);
	string str = to_string(st.wYear) + "-" + to_string(st.wMonth) + "-" + to_string(st.wDay) + " " + to_string(st.wHour) + ":" + to_string(st.wMinute) + ":" + to_string(st.wSecond);
	return str;
}


int main()
{
	const char IP_SERV[] = "127.0.0.1";
	const int PORT_NUM = 80;
	const short BUFF_SIZE = 1024;
	int EC{}; //Error Check

	WSAData wsData; 

	EC = WSAStartup(MAKEWORD(2, 2), &wsData);
	if (EC != 0)
	{
		cerr << "Initialization error " << WSAGetLastError() << endl;
		WSACleanup();
		return EXIT_FAILURE;
	}

	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == INVALID_SOCKET)
	{
		cerr << "Socket created failed " << WSAGetLastError() << endl;
		closesocket(serverSocket);
		WSACleanup();
		return EXIT_FAILURE;
	}

	in_addr ipServer;
	EC = inet_pton(AF_INET, IP_SERV, &ipServer);
	if (EC < 0)
	{
		cerr << "Error ip transform " << endl;
	}

	sockaddr_in servInfo;
	ZeroMemory(&servInfo, sizeof(servInfo));
	servInfo.sin_family = AF_INET;
	servInfo.sin_port = htons(PORT_NUM);
	servInfo.sin_addr = ipServer;

	//servInfo.sin_addr.s_addr = INADDR_ANY;

	EC = bind(serverSocket, (sockaddr*)&servInfo, sizeof(servInfo));
	if (EC != 0)
	{
		cerr << "Error binding " << WSAGetLastError() << endl;
		closesocket(serverSocket);
		WSACleanup();
		return EXIT_FAILURE;
	}

	if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		cerr << "Error listening " << WSAGetLastError() << endl;
		closesocket(serverSocket);
		WSACleanup();
		return EXIT_FAILURE;
	}
	else
	{
		cout << "Listening...\n";
	}

	cout << "Main thread #: " << std::this_thread::get_id() << endl;

	while (true)
	{
		sockaddr_in clientInfo;
		ZeroMemory(&clientInfo, sizeof(clientInfo));
		int size = sizeof(clientInfo);

		SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientInfo, &size);

		if (clientSocket == INVALID_SOCKET)
		{
			cerr << "Client detected, but can't connect to a client. Error # " << WSAGetLastError() << endl;
			closesocket(clientSocket);
			break;
		}
		else
		{
			char clientIP[22];
			inet_ntop(AF_INET, &clientInfo.sin_addr, clientIP, INET_ADDRSTRLEN); //адрес в строку
			cout << "Client connected with IP address " << clientIP << endl;
		}

		std::thread th(work_with_client, clientSocket, BUFF_SIZE);
		th.detach();

	}

	closesocket(serverSocket);
	WSACleanup();

	return EXIT_SUCCESS;
}

void work_with_client(SOCKET cs, const short BUFF_SIZE)
{
	cout << "Client thread #: " << std::this_thread::get_id() << endl;

	int QB{};

	while (true)
	{
		std::vector<char> BUFF(BUFF_SIZE);
		std::vector<std::string> request;

		QB = recv(cs, BUFF.data(), BUFF.size(), 0);

		if (QB < 0)
		{
			cerr << "Error write " << WSAGetLastError() << endl;
			break;
		}

		else if (QB == 0)
		{
			cout << "Client disconnect" << endl;
			return;
		}

		else
		{
			std::string buff{ BUFF.begin(), BUFF.begin() + QB };
			std::string header;

			for (auto ch : buff)
			{
				if (ch != '\r' && ch != '\n')
				{
					header.push_back(ch);
				}
				else if (ch == '\n')
				{
					request.push_back(header);
					header.clear();
				}
			}

			if (request.front().find("GET / ") != std::string::npos)
			{
				std::stringstream ss;
				std::string data = siteData("index.html");

				ss << "HTTP/1.1 200 OK\r\n"
					<< "Date: " << getTime() << "\r\n"
					<< "Content-Type: text/plain\r\n"
					<< "Content-Length: " << data.size() << "\r\n"
					<< "Connection: keep-alive\r\n"
					<< "Server: 127.0.0.1\r\n\r\n"
					<< data;

				std::string response = ss.str();

				if (send(cs, response.data(), response.size(), 0) == INVALID_SOCKET)
				{
					cerr << "Dont send message";
				}
			}
			else if (request.front().find("GET /image ") != std::string::npos)
			{
				std::stringstream ss;
				std::string data = siteData("test.png");

				ss << "HTTP/1.1 200 OK\r\n"
					<< "Date: " << getTime() << "\r\n"
					<< "Content-Type: image/png\r\n"
					<< "Content-Length: " << data.size() << "\r\n"
					<< "Connection: keep-alive\r\n"
					<< "Server: 127.0.0.1\r\n\r\n"
					<< data;

				std::string response = ss.str();

				if (send(cs, response.data(), response.size(), 0) == INVALID_SOCKET)
				{
					cerr << "Dont send message";
				}
			}
			else if (request.front().find("GET /site") != std::string::npos)
			{
				size_t begin{}, end{};

				begin = request.front().find("site");
				end = request.front().find(" ", begin);

				std::string path = request.front().substr(begin, end - begin);
				std::string data = siteData(path);

				if (data == "Not Found")
				{
					std::stringstream ss;

					ss << "HTTP/1.1 404 Not Found\r\n"
						<< "Date: " << getTime() << "\r\n"
						<< "Content-Type: text/plain\r\n"
						<< "Content-Length: " << data.size() << "\r\n"
						<< "Connection: keep-alive\r\n"
						<< "Server: 127.0.0.1\r\n\r\n"
						<< data;

					std::string response = ss.str();

					QB = send(cs, response.data(), response.size(), 0);

					if (QB == INVALID_SOCKET)
					{
						cerr << "Dont send message";
					}
				}
				else
				{
					std::stringstream ss;

					ss << "HTTP/1.1 200 OK\r\n"
						<< "Date: " << getTime() << "\r\n"
						<< "Content-Type: text/plain\r\n"
						<< "Content-Length: " << data.size() << "\r\n"
						<< "Connection: keep-alive\r\n"
						<< "Server: 127.0.0.1\r\n\r\n"
						<< data;

					std::string response = ss.str();

					QB = send(cs, response.data(), response.size(), 0);

					if (QB == INVALID_SOCKET)
					{
						cerr << "Dont send message";
					}
				}
			}
			else if (request.front().find("GET /doc") != std::string::npos)
			{
				size_t begin{}, end{};

				begin = request.at(1).find("site");

				std::string path = request.at(1).substr(begin); //считает от begin и до конца строки

				begin = request.front().find("/doc");
				end = request.front().find(" ", begin);

				path += request.front().substr(begin, end - begin);

				std::string data = siteData(path);

				if (data == "Not Found")
				{
					std::stringstream ss;

					ss << "HTTP/1.1 404 Not Found\r\n"
						<< "Date: " << getTime() << "\r\n"
						<< "Content-Type: text/plain\r\n"
						<< "Content-Length: " << data.size() << "\r\n"
						<< "Connection: keep-alive\r\n"
						<< "Server: 127.0.0.1\r\n\r\n"
						<< data;

					std::string response = ss.str();

					QB = send(cs, response.data(), response.size(), 0);

					if (QB == INVALID_SOCKET)
					{
						cerr << "Dont send message";
					}
				}
				else
				{
					std::stringstream ss;

					ss << "HTTP/1.1 200 OK\r\n"
						<< "Date: " << getTime() << "\r\n"
						<< "Content-Type: text/plain\r\n"
						<< "Content-Length: " << data.size() << "\r\n"
						<< "Connection: keep-alive\r\n"
						<< "Server: 127.0.0.1\r\n\r\n"
						<< data;

					std::string response = ss.str();

					QB = send(cs, response.data(), response.size(), 0);

					if (QB == INVALID_SOCKET)
					{
						cerr << "Dont send message";
					}
				}
			}
			else if (request.front().find("POST / ") != std::string::npos)
			{
				size_t pos = buff.find("\r\n\r\n");
				if (pos != std::string::npos)
				{
					std::string body = buff.substr(pos + 4);
					std::cout << "POST data: " << body << std::endl;
				}

				std::stringstream ss;
				std::string data{ "Your POST request received" };

				ss << "HTTP/1.1 200 OK\r\n"
					<< "Date: " << getTime() << "\r\n"
					<< "Content-Type: text/plain\r\n"
					<< "Content-Length: " << data.size() << "\r\n"
					<< "Connection: keep-alive\r\n"
					<< "Server: 127.0.0.1\r\n\r\n"
					<< data;

				std::string response = ss.str();

				QB = send(cs, response.data(), response.size(), 0);

				if (QB == INVALID_SOCKET)
				{
					cerr << "Dont send message";
				}
			}
			else if (request.front().find("HEAD / ") != std::string::npos)
			{
				std::stringstream ss;

				ss << "HTTP/1.1 200 OK\r\n"
					<< "Date: " << getTime() << "\r\n"
					<< "Content-Type: text/plain\r\n"
					<< "Connection: keep-alive\r\n"
					<< "Server: 127.0.0.1\r\n\r\n";

				std::string response = ss.str();

				if (send(cs, response.data(), response.size(), 0) == INVALID_SOCKET)
				{
					cerr << "Dont send message";
				}
			}
			else if (request.front().find("PUT / ") != std::string::npos)
			{
				size_t pos = buff.find("\r\n\r\n");

				if (pos != std::string::npos)
				{
					std::string body = buff.substr(pos + 4);
					if (writeData(body))
					{
						cout << "Message is write successful" << endl;
					}
				}

				std::stringstream ss;
				std::string data{ "Your PUT request received" };

				ss << "HTTP/1.1 200 OK\r\n"
					<< "Date: " << getTime() << "\r\n"
					<< "Content-Type: text/plain\r\n"
					<< "Content-Length: " << data.size() << "\r\n"
					<< "Connection: keep-alive\r\n"
					<< "Server: 127.0.0.1\r\n\r\n"
					<< data;

				std::string response = ss.str();

				QB = send(cs, response.data(), response.size(), 0);

				if (QB == INVALID_SOCKET)
				{
					cerr << "Dont send message";
				}
			}
			else if (request.front().find("DELETE /") != std::string::npos)
			{
				size_t begin = request.front().find("/");
				size_t end = request.front().find(" ", begin);
				std::string path = request.front().substr(begin + 1, end - begin - 1);

				if (deleteFile(path))
				{
					std::stringstream ss;
					std::string data{ "Your DELETE request received" };

					ss << "HTTP/1.1 200 OK\r\n"
						<< "Date: " << getTime() << "\r\n"
						<< "Content-Type: text/plain\r\n"
						<< "Content-Length: " << data.size() << "\r\n"
						<< "Connection: keep-alive\r\n"
						<< "Server: 127.0.0.1\r\n\r\n"
						<< data;

					std::string response = ss.str();

					QB = send(cs, response.data(), response.size(), 0);

					if (QB == INVALID_SOCKET)
					{
						cerr << "Dont send message";
					}
				}
				else
				{
					std::stringstream ss;
					std::string data{ "File Not Found" };

					ss << "HTTP/1.1 404 Not Found\r\n"
						<< "Date: " << getTime() << "\r\n"
						<< "Content-Type: text/plain\r\n"
						<< "Content-Length: " << data.size() << "\r\n"
						<< "Connection: keep-alive\r\n"
						<< "Server: 127.0.0.1\r\n\r\n"
						<< data;

					std::string response = ss.str();

					QB = send(cs, response.data(), response.size(), 0);

					if (QB == INVALID_SOCKET)
					{
						cerr << "Dont send message";
					}
				}				
				}
			else if (request.front().find("OPTIONS") != std::string::npos)
			{
				std::stringstream ss;
				std::string data{ "GET,HEAD,POST,PUT,DELETE,TRACE" };

				ss << "HTTP/1.1 200 OK\r\n"
					<< "Date: " << getTime() << "\r\n"
					<< "Content-Type: text/plain\r\n"
					<< "Content-Length: " << data.size() << "\r\n"
					<< "Connection: keep-alive\r\n"
					<< "Server: 127.0.0.1\r\n\r\n"
					<< data;

				std::string response = ss.str();

				if (send(cs, response.data(), response.size(), 0) == INVALID_SOCKET)
				{
					cerr << "Dont send message";
				}
			}
			else if (request.front().find("TRACE") != std::string::npos)
			{
				std::stringstream ss, body;

				ss << "HTTP/1.1 200 OK\r\n"
					<< "Date: " << getTime() << "\r\n"
					<< "Content-Type: message/http\r\n\r\n";

				for (const auto str : request)
				{
					body << str << "\r\n";
				}

				ss << body.rdbuf();

				std::string response = ss.str();
				if (send(cs, response.data(), response.size(), 0) == INVALID_SOCKET)
				{
					cerr << "Dont send message";
				}
			}
			else
			{
				std::string data{ "\nNot Implemented (Error 501), maybe later...\n" };
				std::stringstream ss;

				ss << "HTTP/1.1 501 Not Emplemented\r\n"
					<< "Date: " << getTime() << "\r\n"
					<< "Content-Type: text/plain\r\n"
					<< "Content-Length: " << data.size() << "\r\n"
					<< "Connection: keep-alive\r\n"
					<< "Server: 127.0.0.1\r\n\r\n"
					<< data;

				std::string response = ss.str();

				QB = send(cs, response.data(), response.size(), 0);

				if (QB == INVALID_SOCKET)
				{
					cerr << "Dont send message";
				}
			}
		}
	}
}

std::string siteData(std::string path)
{
	std::ifstream file(path, std::ios::binary);

	if (!file.is_open())
	{
		std::cerr << "Error opening file" << std::endl;
		return std::string{ "Not Found" };
	}

	std::string data((std::istreambuf_iterator<char>(file)), {});

	file.close();

	return data;
}

bool writeData(std::string data)
{
	std::ofstream file("from PUT.txt", std::ios::app);

	if (!file.is_open())
	{
		std::cerr << "Error oppening file, msg dont write" << endl;
		return false;
	}
	
	file << data << std::endl;

	if (!file)
	{
		std::cerr << "Error writing to file, message not written" << std::endl;
		return false;
	}

	file.close();
	return true;
}

bool deleteFile(const std::string& filename) 
{
	try 
	{
		std::filesystem::remove(filename);
		std::cout << "File successfully deleted" << std::endl;
		return true;
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		std::cerr << "Error deleting file: " << e.what() << std::endl;
		return false;
	}
}