#include "pch.h"
#include "RelayStats.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

// IOCP 워커 스레드에서 기록, 메인 스레드에서 1초마다 가져감
static mutex GLatencyLock;
static vector<int64> GLatencyUs;
static atomic<int64> GRelayRecvCount = 0;

int64 RelayStats::NowUs()
{
	return chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now().time_since_epoch()).count();
}

void RelayStats::StampSendTime(string& payload)
{
	int64 now = NowUs();
	::memcpy(&payload[0], &now, sizeof(int64));
}

void RelayStats::OnReceived(const string& payload)
{
	if (payload.size() < sizeof(int64))
		return;

	int64 sentUs = 0;
	::memcpy(&sentUs, payload.data(), sizeof(int64));
	int64 latencyUs = NowUs() - sentUs;

	GRelayRecvCount.fetch_add(1);

	lock_guard<mutex> guard(GLatencyLock);
	GLatencyUs.push_back(latencyUs);
}

vector<int64> RelayStats::TakeSamples()
{
	vector<int64> samples;
	lock_guard<mutex> guard(GLatencyLock);
	samples.swap(GLatencyUs);
	return samples;
}

int64 RelayStats::RecvCount()
{
	return GRelayRecvCount.load();
}

void RelayStats::Reset()
{
	GRelayRecvCount.store(0);
	TakeSamples();
}

string RelayStats::FormatLatency(vector<int64>& samples)
{
	if (samples.empty())
		return "no samples";

	sort(samples.begin(), samples.end());
	auto percentile = [&](int32 p) -> double
		{
			size_t index = min(samples.size() - 1, samples.size() * p / 100);
			return samples[index] / 1000.0;
		};

	ostringstream ss;
	ss << fixed << setprecision(2)
		<< "p50 " << percentile(50) << "ms / p99 " << percentile(99) << "ms / max " << samples.back() / 1000.0 << "ms";
	return ss.str();
}
