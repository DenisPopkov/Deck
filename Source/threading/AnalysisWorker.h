#pragma once

#include <juce_core/juce_core.h>

namespace cassette
{

class AnalysisWorker
{
public:
    using Job = std::function<void()>;

    AnalysisWorker();
    ~AnalysisWorker();

    void enqueue(Job job);

private:
    class WorkerThread : public juce::Thread
    {
    public:
        explicit WorkerThread(AnalysisWorker& owner) : juce::Thread("analysis-worker"), o(owner) {}
        void run() override;

    private:
        AnalysisWorker& o;
    };

    WorkerThread worker { *this };
    juce::WaitableEvent wake;
    juce::CriticalSection lock;
    juce::Array<Job> queue;

    void runLoop();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalysisWorker)
};

}
