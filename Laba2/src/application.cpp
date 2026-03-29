#include "lms.hpp"

#include <Poco/Data/SQLite/Connector.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Util/Application.h>

#include <cstdlib>

using Poco::Net::HTTPServer;
using Poco::Net::HTTPServerParams;
using Poco::Net::ServerSocket;

namespace lms
{

LearningManagementApplication::LearningManagementApplication():
    _database(resolveDatabasePath())
{
}

void LearningManagementApplication::initialize(Poco::Util::Application& self)
{
    Poco::Data::SQLite::Connector::registerConnector();
    Poco::Util::ServerApplication::initialize(self);
    _database.initialize();
}

void LearningManagementApplication::uninitialize()
{
    Poco::Data::SQLite::Connector::unregisterConnector();
    Poco::Util::ServerApplication::uninitialize();
}

int LearningManagementApplication::main(const std::vector<std::string>&)
{
    const int port = resolvePort();

    ServerSocket socket(port);
    auto params = new HTTPServerParams();
    params->setMaxQueued(100);
    params->setMaxThreads(8);

    HTTPServer server(new ApiRequestHandlerFactory(_database), socket, params);
    server.start();

    logger().information("Learning Management API started on port " + std::to_string(port));
    logger().information("SQLite database: " + resolveDatabasePath());
    waitForTerminationRequest();
    server.stop();

    return Poco::Util::Application::EXIT_OK;
}

int LearningManagementApplication::resolvePort()
{
    const char* rawPort = std::getenv("PORT");
    if (rawPort == nullptr)
    {
        return 8080;
    }

    try
    {
        return std::stoi(rawPort);
    }
    catch (...)
    {
        return 8080;
    }
}

std::string LearningManagementApplication::resolveDatabasePath()
{
    const char* rawPath = std::getenv("DB_PATH");
    if (rawPath == nullptr || trim(rawPath).empty())
    {
        return "data/lms.db";
    }
    return rawPath;
}

} // namespace lms
