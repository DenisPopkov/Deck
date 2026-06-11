#include "AnalysisWorker.h"

namespace cassette
{

void AnalysisWorker::WorkerThread::run() { o.runLoop(); }

AnalysisWorker::AnalysisWorker()
{
    worker.startThread();
}

AnalysisWorker::~AnalysisWorker()
{
    worker.signalThreadShouldExit();
    wake.signal();
    worker.stopThread(2000);
}

void AnalysisWorker::enqueue(Job job)
{
    {
        const juce::ScopedLock sl(lock);
        queue.add(std::move(job));
    }
    wake.signal();
}

void AnalysisWorker::runLoop()
{
    while (!worker.threadShouldExit())
    {
        wake.wait(250);

        Job job;
        {
            const juce::ScopedLock sl(lock);
            if (queue.isEmpty())
                continue;
            job = queue.removeAndReturn(0);
        }

        if (job)
            job();
    }
}

}
