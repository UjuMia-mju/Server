#include "pch.h"
#include "ConfigManager.h"
#include <fstream>
#include <sstream>
#include <map>

std::wstring MultiByteToWide(const std::string& str)
{
	if (str.empty()) return L"";

	int size = ::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
	if (size == 0) return L"";

	std::wstring result(size - 1, 0);
	::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
	return result;
}

std::string WideToMultiByte(const std::wstring& wstr)
{
	if (wstr.empty()) return "";

	int size = ::WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (size == 0) return "";

	std::string result(size - 1, 0);
	::WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr, nullptr);
	return result;
}

std::string Trim(const std::string& str)
{
	size_t first = str.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) return "";
	size_t last = str.find_last_not_of(" \t\r\n");
	return str.substr(first, (last - first + 1));
}

bool ConfigManager::LoadConfig(const std::wstring& configPath)
{
	// 절대 경로 얻기
	WCHAR fullPath[MAX_PATH] = { 0 };
	::GetFullPathNameW(configPath.c_str(), MAX_PATH, fullPath, nullptr);
	_configPath = fullPath;

	wcout << L"Loading config from: " << _configPath << endl;

	// UTF-8 파일 열기
	std::string narrowPath = WideToMultiByte(_configPath);
	std::ifstream file(narrowPath);

	if (!file.is_open())
	{
		wcout << L"Failed to open config file!" << endl;
		return false;
	}

	std::map<std::string, std::string> dbConfig;
	std::map<std::string, std::string> serverConfig;
	std::map<std::string, std::string>* currentMap = nullptr;

	std::string line;
	while (std::getline(file, line))
	{
		// Trim
		line = Trim(line);

		// 빈 줄이나 주석 무시
		if (line.empty() || line[0] == ';' || line[0] == '#')
			continue;

		// 섹션 확인
		if (line == "[Database]")
		{
			currentMap = &dbConfig;
			continue;
		}
		else if (line == "[Server]")
		{
			currentMap = &serverConfig;
			continue;
		}

		// Key=Value 파싱
		size_t pos = line.find('=');
		if (pos != std::string::npos && currentMap != nullptr)
		{
			std::string key = Trim(line.substr(0, pos));
			std::string value = Trim(line.substr(pos + 1));

			(*currentMap)[key] = value;
		}
	}

	file.close();

	// Database 설정 확인
	if (dbConfig.empty())
	{
		wcout << L"ERROR: Database config is empty!" << endl;
		return false;
	}

	// DB 연결 문자열 생성
	std::wstring driver = MultiByteToWide(dbConfig["Driver"]);
	std::wstring server = MultiByteToWide(dbConfig["Server"]);
	std::wstring database = MultiByteToWide(dbConfig["Database"]);
	std::wstring uid = MultiByteToWide(dbConfig["UID"]);
	std::wstring pwd = MultiByteToWide(dbConfig["PWD"]);
	int32 dbPort = std::stoi(dbConfig["Port"]);

	std::wstringstream ss;
	ss << L"DRIVER={" << driver << L"};"
		<< L"SERVER=" << server << L";"
		<< L"PORT=" << dbPort << L";"
		<< L"DATABASE=" << database << L";"
		<< L"UID=" << uid << L";"
		<< L"PWD=" << pwd << L";";

	_dbConnectionString = ss.str();

	// Server 설정
	_serverIP = MultiByteToWide(serverConfig["IP"]);
	_serverPort = static_cast<uint16>(std::stoi(serverConfig["Port"]));
	_workerThreads = std::stoi(serverConfig["WorkerThreads"]);

	return true;
}

std::wstring ConfigManager::GetDBConnectionString()
{
	return _dbConnectionString;
}

std::wstring ConfigManager::GetServerIP()
{
	return _serverIP;
}

uint16 ConfigManager::GetServerPort()
{
	return _serverPort;
}

int32 ConfigManager::GetWorkerThreadCount()
{
	return _workerThreads;
}