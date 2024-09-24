#pragma once
#include <type_traits>


namespace SOEP
{
	class ThreadPool
	{
	public:
		ThreadPool(size_t thread_count = std::thread::hardware_concurrency())
			: m_ExitFlag(false), m_ActiveTasks(0)
		{
			for (size_t i = 0; i < thread_count; ++i) {
				m_Threads.emplace_back([this]() { this->ThreadLoop(); });
			}
		}

		~ThreadPool() { Shutdown(); }

		template<typename F, typename... Args>
		auto AddTask(F&& func, Args&&... args)
			-> std::future<typename std::invoke_result_t<F, Args...>>
		{
			using ReturnType = typename std::invoke_result_t<F, Args...>;

			auto task = std::make_shared<std::packaged_task<ReturnType()>>(
				std::bind(std::forward<F>(func), std::forward<Args>(args)...)
			);
			std::future<ReturnType> result = task->get_future();
			{
				std::lock_guard<std::mutex> lock(m_QueueMutex);
				m_TaskQueue.emplace([task]() { (*task)(); });
				++m_ActiveTasks;
			}
			m_Condition.notify_one();
			return result;
		}

		void Await()
		{
			std::unique_lock<std::mutex> lock(m_QueueMutex);
			m_AwaitCondition.wait(lock, [this]() { return m_ActiveTasks == 0 && m_TaskQueue.empty(); });
		}

		void Shutdown()
		{
			{
				std::lock_guard<std::mutex> lock(m_QueueMutex);
				m_ExitFlag = true;
			}
			m_Condition.notify_all();
			for (std::thread& thread : m_Threads) {
				if (thread.joinable()) {
					thread.join();
				}
			}
		}

	private:
		void ThreadLoop()
		{
			while (true) {
				std::function<void()> task;

				{
					std::unique_lock<std::mutex> lock(m_QueueMutex);
					m_Condition.wait(lock, [this]() { return m_ExitFlag || !m_TaskQueue.empty(); });
					if (m_ExitFlag && m_TaskQueue.empty()) {
						return;
					}
					if (!m_TaskQueue.empty()) {
						task = std::move(m_TaskQueue.front());
						m_TaskQueue.pop();
					}
				}

				if (task) {
					task();
					{
						std::lock_guard<std::mutex> lock(m_QueueMutex);
						--m_ActiveTasks;
						if (m_ActiveTasks == 0 && m_TaskQueue.empty()) {
							m_AwaitCondition.notify_all();
						}
					}
				}
			}
		}

		std::vector<std::thread> m_Threads;
		std::queue<std::function<void()>> m_TaskQueue;
		std::mutex m_QueueMutex;
		std::condition_variable m_Condition;
		std::condition_variable m_AwaitCondition;
		std::atomic<bool> m_ExitFlag;
		std::atomic<size_t> m_ActiveTasks;
	};
} // namespace SOEP
