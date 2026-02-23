#pragma once
#include <string>

class ConfigManager
{
public:
	ConfigManager() = default;
	~ConfigManager() = default;

	bool LoadConfig(const std::wstring& configPath);
	std::wstring GetDBConnectionString();

	// Server settings
	std::wstring GetServerIP();
	uint16 GetServerPort();
	int32 GetWorkerThreadCount();

private:
	std::wstring _configPath;
	std::wstring _dbConnectionString;
	std::wstring _serverIP;
	uint16 _serverPort = 7777;
	int32 _workerThreads = 5;
};