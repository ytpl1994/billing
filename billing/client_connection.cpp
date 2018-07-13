#include "inc/client_connection.hpp"
#include "inc/billing_server.hpp"
#include <iostream>
#include "inc/hex_tool.hpp"
using std::cout;
using std::endl;

ClientConnection::ClientConnection(BillingServer * s, asio::io_service& io) :server(s), socket(io) {
#ifdef OPEN_SERVER_DEBUG
	Logger::write("client construct");
#endif
}

ClientConnection::~ClientConnection()
{
#ifdef OPEN_SERVER_DEBUG
	Logger::write("client destroy");
#endif
}

tcp::socket & ClientConnection::getSocket()
{
	return socket;
}
void ClientConnection::acceptHandler(const asio::error_code & error)
{

#ifdef OPEN_SERVER_DEBUG
	Logger::write("connection entered");
	Logger::write("prepare accept new");
#endif
	server->startAccept();
	if (error) {
#ifdef OPEN_SERVER_DEBUG
		Logger::write(string("accept error ") + error.message());
#endif
	}
	else {
		socket.set_option(asio::socket_base::keep_alive(true));
		this->readFromClient();
	}
}

void ClientConnection::writeHandler(const asio::error_code & error, std::size_t size)
{
	if (error) {
#ifdef OPEN_SERVER_DEBUG
		Logger::write(string("write error ") + error.message());
#endif
	}
}

void ClientConnection::readFromClient()
{
	if (this->server->stopMask) {
		cout << "stop server read" << endl;
	}
#ifdef OPEN_SERVER_DEBUG
	Logger::write("read client start");
#endif
	auto request = std::make_shared<vector<char>>();
	request->resize(260);
	auto selfPointer(shared_from_this());
	socket.async_receive(
		asio::buffer(*request),
		[this, selfPointer, request](const asio::error_code& error, std::size_t size) {
#ifdef OPEN_SERVER_DEBUG
		Logger::write("read client end");
#endif
		//读取错误
		if (error) {
#ifdef OPEN_SERVER_DEBUG
			Logger::write(string("read client error: ") + error.message());
#endif
		}
		else {
			//开启下一次读取
			this->readFromClient();
			if (size > 0) {
				//移除尾部多余空间
				request->resize(size);
				this->processRequest(request, size);
			}
		}
	});

}

void ClientConnection::processRequest(std::shared_ptr<vector<char>> request, std::size_t size)
{
#ifdef OPEN_SERVER_DEBUG
	Logger::write(string("request data[length:") + std::to_string(size) + "]");
	string requestHexDebug;
	bytesToHexDebug(*request, requestHexDebug);
	Logger::write(requestHexDebug);
#endif
	//判断是否为命令
	if (request->size()==4) {
		string commandHex;
		bytesToHex(*request, commandHex);
		//close命令
		if (commandHex.compare("00000000") == 0) {
			this->server->stopMask = true;
			cout << "get command : stop" << endl;
			this->server->ioService.stop();
			return;
		}
	}
	BillingData requestData(request);
	if (requestData.isDataValid()) {
		unsigned char requestType = requestData.getPayloadType();
		auto it = server->handlers.find(requestType);
		if (it != server->handlers.end()) {
			this->processRequest((*it).second, requestData);
		}
	}
	else {
		cout << "not valid BillingData" << endl;
	}
}

void ClientConnection::processRequest(std::shared_ptr<RequestHandler> handler, BillingData & requestData)
{
	cout << "request billing data" << endl;
	string debugStr;
	requestData.doDump(debugStr);
	cout << debugStr << endl;
	BillingData responseData;
	handler->processRequest(requestData, responseData);
	if (responseData.isDataValid()) {
		auto selfPointer(shared_from_this());
#ifdef OPEN_SERVER_DEBUG
		Logger::write("write client start");
#endif
		vector<char> resp;
		responseData.packData(resp);
		socket.async_send(
			asio::buffer(resp),
			[this, selfPointer, &resp](const asio::error_code& error, std::size_t size) {

#ifdef OPEN_SERVER_DEBUG
			Logger::write("write client end");
			string hexStr;
			bytesToHex(resp,hexStr);
			Logger::write(string("response data\r\n") + hexStr);
#endif
			this->writeHandler(error, size);
		}
		);
	}
}
