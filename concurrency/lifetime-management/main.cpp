#include <thread>
#include <functional>
#include <mutex>
#include <memory>
#include <iostream>
#include <future>
#include <list>
#include <fstream>

using namespace std;
using namespace std::chrono;

typedef function<void(int result)> Callback;

void LaunchConcurrentProcess(const Callback & callback)
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

// Клиентский объект, который обновляет свое состояние асинхронно, наивно полагая, что его время жизни 
// будет дольше, чем время работы фонового процесса
struct BuggyClient
{
	void UpdateStateConcurrently()
	{
		LaunchConcurrentProcess([this](int value){
			// Этот код опасен. Объект может разрушиться к моменту завершения параллельного процесса
			lock_guard<mutex> lk(m_mutex);
			m_state = value;
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

// Клиент, допускающий свое разрушение до окончания фонового процесса
// Для поддержки 
struct GoodClient 
	: enable_shared_from_this<GoodClient> // Для поддержки shared_from_this()
{
	static shared_ptr<GoodClient> Create()
	{
		return shared_ptr<GoodClient>(new GoodClient);
	}

	void UpdateStateConcurrently()
	{
		weak_ptr<GoodClient> weakThis = shared_from_this();
		// В лямбде захватываем слабую ссылку на самих себя, чтобы избежать циклических ссылок
		LaunchConcurrentProcess([weakThis](int value){
			if (auto strongThis = weakThis.lock()) // Проверяем, живы ли мы (слабая ссылка обнулится перед разрушением)
			{
				lock_guard<mutex> lk(strongThis->m_mutex);
				strongThis->m_state = value;
			}
		});
	}

	int GetState()const
	{
		lock_guard<mutex> lk(m_mutex);
		return m_state;
	}
private:
	// Приватный конструктор, обеспечивающий создание обернутого в shared_ptr экземпляра только через статический метод Create
	GoodClient() = default;

	// Обычно для таких объектов операция копирования и присваивания должна быть недоступной
	GoodClient(const GoodClient&) = delete;
	GoodClient& operator = (const GoodClient&) = delete;

	mutable mutex m_mutex;
	int m_state = 0;
};

// Клиент, допускающий свое разрушение до окончания фонового процесса
// В отличие от GoodClient не требует явного создания себя в куче (за счет использования идиомы pimpl)
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
	// Обычно для таких объектов операция копирования и присваивания должна быть недоступной
	BetterClient(const BetterClient&) = delete;
	BetterClient& operator = (const BetterClient&) = delete;

	// Реализация объекта, управляемая shared_ptr-ом внутри внешнего объекта.
	struct Impl : enable_shared_from_this<Impl>
	{
		void UpdateStateConcurrently()
		{
			weak_ptr<Impl> weakThis = shared_from_this();
			// В лямбде захватываем слабую ссылку на самих себя, чтобы избежать циклических ссылок
			LaunchConcurrentProcess([weakThis](int value){
				if (auto strongThis = weakThis.lock()) // Проверяем, живы ли мы
				{
					lock_guard<mutex> lk(strongThis->m_mutex);
					strongThis->m_state = value;
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

future<void> LaunchConcurrentProcessWithFuture(const Callback & callback)
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

// Этот объект ожидает при своем разрушении окончания порождаемых им фоновых процессов
struct WaitingClient
{
	WaitingClient() = default;

	void UpdateStateConcurrently()
	{
		m_worker = LaunchConcurrentProcessWithFuture([this](int value){
			lock_guard<mutex> lk(m_mutex);
			m_state = value;
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
	WaitingClient(const WaitingClient&) = delete;
	WaitingClient& operator = (const WaitingClient&) = delete;

	future<void> m_worker;
	mutable mutex m_mutex;
	int m_state = 0;
};

void main()
{
	{
		// Код ниже небезопасен. переменная file может разрушиться до окончания асинхронного процесса
		/*
		ofstream file("output.txt");
		LaunchConcurrentProcess([&file](int value){
			file << value << endl;
		});
		*/
	}

	{
		// Контекст через shared_ptr захватывается лямбда-функцией и будет доступен 
		// все время ее существования (до конца фонового процесса)
		auto file = make_shared<ofstream>("output.txt");
		LaunchConcurrentProcess([file](int value){
			*file << value << endl;
		});
	}

	{
		// Код ниже опасен. Объект, порождающий фоновый поток может быть разрушен до окончания получения уведомлений
		// от фонового потока.
		// Использование boost::signals2::slot вместо функции обратного вызова могло бы быть быть альтернативным решением,
		// только нужно обеспечить своевременный дисконнект от сигнала в деструкторе, чтобы не получать уведомления
		// при частично разрушенном объекте, но еще не разрушенных полях scoped_connection
		/*
		BuggyClient client;
		client.UpdateStateConcurrently();
		this_thread::sleep_for(milliseconds(1000)); // Наивно надеемся, что 1000 мс будет достаточно, чтобы закончить работу
		cout << "Good client state" << client.GetState() << endl;
		*/
	}

	{
		auto client = GoodClient::Create();
		client->UpdateStateConcurrently();
		this_thread::sleep_for(milliseconds(1000));
		cout << "Good client state" << client->GetState() << endl;
	}

	{
		// BetterClient скрывает внутри себя управление временем жизни через shared_ptr<Impl>
		BetterClient client;
		client.UpdateStateConcurrently();
		this_thread::sleep_for(milliseconds(1000));
		cout << "Better client state" << client.GetState() << endl;
	}

	{
		// Этот объект при своем разрушении дождется завершения порождаемых им фоновых процессов
		WaitingClient client;
		client.UpdateStateConcurrently();
		this_thread::sleep_for(milliseconds(1000));
		cout << "Waiting client state" << client.GetState() << endl;
	}
	
}
