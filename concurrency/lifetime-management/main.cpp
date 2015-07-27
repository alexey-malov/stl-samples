#include <thread>
#include <functional>
#include <mutex>
#include <memory>
#include <iostream>
#include <future>
#include <list>

using namespace std;
using namespace std::chrono;

typedef function<void(int result)> Callback;
void FindNewStateInDetachedThread(const Callback & callback)
{
	thread([=]{
		int value = 0;
		while (++value <= 42)
		{
			this_thread::sleep_for(milliseconds(1000/42));
			callback(value);
		}
	}).detach();
}

// Client that doesn't take care 
struct BuggyClient
{
	void UpdateStateConcurrently()
	{
		FindNewStateInDetachedThread([this](int state){
			// Danger. Undefined behavior if object has been destroyed
			lock_guard<mutex> lk(m_mutex);
			m_state = state;
		});
	}

	int GetState()const
	{
		lock_guard<mutex> lk(m_mutex);
		return m_state;
	}
private:
	mutable mutex m_mutex;
	int m_state = 0;
};

struct GoodClient : enable_shared_from_this<GoodClient>
{
	void UpdateStateConcurrently()
	{
		weak_ptr<GoodClient> weakThis = shared_from_this();
		FindNewStateInDetachedThread([weakThis](int state){
			if (auto strongThis = weakThis.lock()) // Verify if object is alive
			{
				lock_guard<mutex> lk(strongThis->m_mutex);
				strongThis->m_state = state;
			}
		});
	}

	int GetState()const
	{
		lock_guard<mutex> lk(m_mutex);
		return m_state;
	}
private:
	mutable mutex m_mutex;
	int m_state = 0;
};

struct BetterClient
{
	BetterClient()
		:m_pImpl(make_shared<Impl>())
	{

	}

	void UpdateStateConcurrently()
	{
		m_pImpl->UpdateStateConcurrently();
	}

	int GetState()const
	{
		return m_pImpl->GetState();
	}
private:
	BetterClient(const BetterClient&) = delete;
	BetterClient& operator = (const BetterClient&) = delete;

	struct Impl : enable_shared_from_this<Impl>
	{
		void UpdateStateConcurrently()
		{
			weak_ptr<Impl> weakThis = shared_from_this();
			FindNewStateInDetachedThread([weakThis](int result){
				if (auto strongThis = weakThis.lock())// Verify if object is alive
				{
					lock_guard<mutex> lk(strongThis->m_mutex);
					strongThis->m_state = result;
				}
			});
		}

		int GetState()const
		{
			lock_guard<mutex> lk(m_mutex);
			return m_state;
		}

	private:
		mutable mutex m_mutex;
		int m_state = 0;
	};
	shared_ptr<Impl> m_pImpl; 
};

future<void> FindNewStateWithFuture(const Callback & callback)
{
	return async(launch::async, [=]{
		int value = 0;
		while (++value <= 42)
		{
			this_thread::sleep_for(milliseconds(1000 / 42));
			callback(value);
		}
	});
}

struct WaitingClient
{
	void UpdateStateConcurrently()
	{
		m_worker = FindNewStateWithFuture([this](int state){
			lock_guard<mutex> lk(m_mutex);
			m_state = state;
		});
	}

	int GetState()const
	{
		lock_guard<mutex> lk(m_mutex);
		return m_state;
	}

	~WaitingClient()
	{
		// Make sure worker the has finished before client is destroyed
		if (m_worker.valid())
		{
			m_worker.get();
		}
	}
private:
	future<void> m_worker;
	mutable mutex m_mutex;
	int m_state = 0;
};


void main()
{
	{
		// The code below is dangerous. Buggy client may be destroyed BEFORE concurrent task is over.
		// In this case accessing data of the destroyed object will cause undefined behavior
		/*
		BuggyClient client;
		client.UpdateStateConcurrently();
		this_thread::sleep_for(milliseconds(1000));
		cout << "Good client state" << client.GetState() << endl;
		*/
	}

	{
		// Warning, the GoodClient MUST BE managed by shared_ptr, otherwise shared_from_this fill fail
		auto client = make_shared<GoodClient>();
		client->UpdateStateConcurrently();
		this_thread::sleep_for(milliseconds(1000));
		cout << "Good client state" << client->GetState() << endl;
	}

	{
		// Better client may have any lifetime policy (auto, shared, unique)
		BetterClient client;
		client.UpdateStateConcurrently();
		this_thread::sleep_for(milliseconds(1000));
		cout << "Better client state" << client.GetState() << endl;
	}

	{
		WaitingClient client;
		client.UpdateStateConcurrently();
		this_thread::sleep_for(milliseconds(1000));
		cout << "Waiting client state" << client.GetState() << endl;
	}
	
}
