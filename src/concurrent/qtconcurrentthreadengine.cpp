// MIT License - see LICENSE
#include <QtConcurrent/qtconcurrentthreadengine.h>

#include <algorithm>
#include <atomic>
#include <climits>

namespace QtPrivate {

namespace {

// Blocks are small enough to share the work evenly between the threads when the items differ
// in cost, and large enough that claiming one does not dominate a cheap function.
constexpr qsizetype kBlocksPerWorker = 8;
constexpr qsizetype kMaxBlock = 256;

class Engine {
public:
    explicit Engine(ConcurrentJob job) : m_job(std::move(job)) {}

    void setBlockSize(qsizetype block) { m_block = block; }
    QFutureInterfaceBase &future() { return m_job.future; }

    // Claims blocks until none is left, the future is canceled or a block throws. Any number of
    // threads may call it, at any time; a thread that comes after the end returns at once.
    void work() {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            if (m_finalized)
                return;
            ++m_running;
        }
        for (;;) {
            if (m_stop.load() || m_job.future.isCanceled())
                break;
            m_job.future.waitForResume();
            if (m_job.future.isCanceled())
                break;
            const qsizetype begin = m_next.fetch_add(m_block);
            if (begin >= m_job.count)
                break;
            const qsizetype end = std::min(begin + m_block, m_job.count);
            try {
                m_job.runBlock(begin, end);
            } catch (...) {
                std::lock_guard<std::mutex> lk(m_mutex);
                if (!m_error)
                    m_error = std::current_exception();
                m_stop.store(true);
                break;
            }
            // Under the lock, so that the progress never goes backwards.
            std::lock_guard<std::mutex> lk(m_mutex);
            m_done += end - begin;
            m_job.future.setProgressValue(static_cast<int>(std::min<qsizetype>(m_done, INT_MAX)));
        }
        // Every reason to leave the loop is permanent, so the thread that brings the count to zero
        // is the last one that had anything to do.
        bool last;
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            last = --m_running == 0 && !m_finalized;
            if (last)
                m_finalized = true;
        }
        if (last)
            finalize();
    }

    // Ends the future. No thread is in runBlock() any more, and none enters it again.
    void finalize() {
        if (m_error) {
            m_job.future.reportException(m_error);
        } else if (m_job.finish && !m_job.future.isCanceled()) {
            try {
                m_job.finish();
            } catch (...) {
                m_job.future.reportException(std::current_exception());
            }
        }
        // Lets go of the functions, and of the sequence they hold, before anyone is told.
        m_job.runBlock = nullptr;
        m_job.finish = nullptr;
        m_job.future.reportFinished();
    }

private:
    ConcurrentJob m_job;
    qsizetype m_block = 1;
    std::atomic<qsizetype> m_next{0};
    std::atomic<bool> m_stop{false};
    std::mutex m_mutex;
    int m_running = 0;
    bool m_finalized = false;
    qsizetype m_done = 0;
    std::exception_ptr m_error;
};

} // namespace

void startConcurrentJob(ConcurrentJob job, bool blocking) {
    QThreadPool *pool = job.pool ? job.pool : QThreadPool::globalInstance();
    const qsizetype count = job.count;
    auto engine = std::make_shared<Engine>(std::move(job));
    QFutureInterfaceBase &future = engine->future();
    future.setThreadPool(pool);
    future.setProgressRange(0, static_cast<int>(std::min<qsizetype>(count, INT_MAX)));
    future.reportStarted();
    if (count <= 0) {
        engine->finalize();
        return;
    }

    const qsizetype workers = std::clamp<qsizetype>(pool->maxThreadCount(), 1, count);
    engine->setBlockSize(std::clamp<qsizetype>(count / (workers * kBlocksPerWorker), 1, kMaxBlock));
    const qsizetype pooled = blocking ? workers - 1 : workers;
    for (qsizetype i = 0; i < pooled; ++i)
        pool->start([engine] { engine->work(); });
    if (blocking)
        engine->work();
}

} // namespace QtPrivate
