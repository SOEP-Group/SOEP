#pragma once
#include "base.h"
#include <chrono>
#include <unordered_map>
#include <spdlog/spdlog.h>

namespace SOEP {

	class Timer
	{
	public:
		SOEP_FORCE_INLINE Timer() { Reset(); }
		SOEP_FORCE_INLINE void Reset() { m_Start = std::chrono::high_resolution_clock::now(); }
		SOEP_FORCE_INLINE float Elapsed() { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - m_Start).count() * 0.001f * 0.001f; }
		SOEP_FORCE_INLINE float ElapsedMillis() { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - m_Start).count() * 0.001f; }
	private:
		std::chrono::time_point<std::chrono::high_resolution_clock> m_Start;
	};

	class ScopedTimer
	{
	public:
		ScopedTimer(const std::string& name)
			: m_Name(name) {}
		~ScopedTimer()
		{
			float time = m_Timer.ElapsedMillis();
			spdlog::trace("{0} - {1}ms", m_Name, time);
		}
	private:
		std::string m_Name;
		Timer m_Timer;
	};

	class PerformanceProfiler
	{
	public:
		struct PerFrameData
		{
			float Time = 0.0f;
			uint32_t Samples = 0;

			PerFrameData() = default;
			PerFrameData(float time) : Time(time) {}

			operator float() const { return Time; }
			inline PerFrameData& operator+=(float time)
			{
				Time += time;
			}
		};
	public:
		void SetPerFrameTiming(const char* name, float time)
		{
			std::scoped_lock<std::mutex> lock(m_PerFrameDataMutex);

			if (m_PerFrameData.find(name) == m_PerFrameData.end())
				m_PerFrameData[name] = 0.0f;

			PerFrameData& data = m_PerFrameData[name];
			data.Time += time;
			data.Samples++;
		}

		void Clear()
		{
			std::scoped_lock<std::mutex> lock(m_PerFrameDataMutex);
			m_PerFrameData.clear();
		}

		const std::unordered_map<const char*, PerFrameData>& GetPerFrameData() const { return m_PerFrameData; }
	private:
		std::unordered_map<const char*, PerFrameData> m_PerFrameData;
		inline static std::mutex m_PerFrameDataMutex;
	};

#if 1

#define SOEP_SCOPE_TIMER(name)\
	ScopedTimer timer__LINE__(name);
#else
#define SOEP_SCOPE_TIMER(name)
#endif
}