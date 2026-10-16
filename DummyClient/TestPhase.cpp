#include "pch.h"
#include "TestPhase.h"
#include "StressTest.h"
#include "Service.h"
#include "ThreadManager.h"
#include <algorithm>
#include <iomanip>
#include <numeric>

using namespace TestPhase;

static const string PASSWORD = "1234";

int64 TestPhase::NowUs()
{
	return chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now().time_since_epoch()).count();
}

vector<Client> TestPhase::CreateClients(int32 count, ostream& out)
{
	count = StressTest::ClampClientCount(count, out);

	vector<Client> clients;
	ClientServiceRef service = MakeShared<ClientService>(
		NetAddress(L"127.0.0.1", 7777),
		MakeShared<IocpCore>(),
		[&]() {
			string email = "player" + to_string(clients.size() + 1) + "@test.com";
			auto client = MakeShared<ServerSession>(email, PASSWORD, false);
			clients.push_back(client);
			return client;
		},
		count);

	for (int32 i = 0; i < 8; i++)
	{
		GThreadManager->Launch([service]()
			{
				while (true)
				{
					service->GetIocpCore()->Dispatch();
				}
			});
	}

	// Start()는 세션을 순서대로 동기 생성하므로, 리턴 후 clients는 더 이상 바뀌지 않음
	ASSERT_CRASH(service->Start());

	auto start = chrono::steady_clock::now();
	int32 connected = 0;
	while (chrono::steady_clock::now() - start < PHASE_TIMEOUT)
	{
		connected = static_cast<int32>(count_if(clients.begin(), clients.end(), [](const Client& c) { return c->IsConnected(); }));
		if (connected == count)
			break;

		this_thread::sleep_for(10ms);
	}

	out << "[Phase] Connected " << connected << " / " << count << endl;
	return clients;
}

vector<Group> TestPhase::MakeGroups(const vector<Client>& clients, int32 size)
{
	vector<Group> groups;
	for (size_t i = 0; i + size <= clients.size(); i += size)
		groups.push_back(Group{ vector<Client>(clients.begin() + i, clients.begin() + i + size) });

	return groups;
}

vector<Client> TestPhase::Hosts(const vector<Group>& groups)
{
	vector<Client> hosts;
	for (auto& group : groups)
		hosts.push_back(group.Host());

	return hosts;
}

vector<Client> TestPhase::AllMembers(const vector<Group>& groups)
{
	vector<Client> members;
	for (auto& group : groups)
		members.insert(members.end(), group.members.begin(), group.members.end());

	return members;
}

static double PercentileMs(vector<int64>& sortedUs, int32 p)
{
	if (sortedUs.empty())
		return 0;

	size_t index = min(sortedUs.size() - 1, sortedUs.size() * p / 100);
	return sortedUs[index] / 1000.0;
}

void TestPhase::Report(vector<PhaseResult> results, ostream& out)
{
	out << "\n========== PHASE REPORT ==========" << endl;
	out << left << setw(8) << "Phase" << right << setw(10) << "Success" << setw(10) << "Total"
		<< setw(10) << "avg" << setw(10) << "p50" << setw(10) << "p99" << setw(10) << "max" << endl;

	out << fixed << setprecision(1);
	for (auto& r : results)
	{
		auto& lat = r.latencyUs;
		sort(lat.begin(), lat.end());
		double avg = lat.empty() ? 0 : accumulate(lat.begin(), lat.end(), 0LL) / 1000.0 / lat.size();
		double maxMs = lat.empty() ? 0 : lat.back() / 1000.0;

		out << left << setw(8) << r.name << right
			<< setw(10) << (to_string(r.success) + "/" + to_string(r.total))
			<< setw(8) << r.elapsedMs << "ms"
			<< setw(8) << avg << "ms" << setw(8) << PercentileMs(lat, 50) << "ms"
			<< setw(8) << PercentileMs(lat, 99) << "ms" << setw(8) << maxMs << "ms" << endl;
	}
	out << defaultfloat;

	for (auto& r : results)
		if (r.success < r.total)
			out << "  * " << r.name << ": " << (r.total - r.success) << " failed or timed out" << endl;
}

void TestPhase::WaitForEnter(const string& message, ostream& out)
{
	out << "\n" << message << endl;
	cin.get();
}
