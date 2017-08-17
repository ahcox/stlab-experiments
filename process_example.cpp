// Copied from http://stlab.cc/libraries/concurrency/channel/process/process_example.cpp
// Note there was no license on that file, no copyright, no authorship when it was copied.
// Modified by Andrew Cox, August 2017.
#include <atomic>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <stlab/concurrency/channel.hpp>
#include <stlab/concurrency/default_executor.hpp>

using namespace stlab;

/**
 * One-shot wait on a condition variable for which the cost of creating a mutex
 * could not have been amortized over many waits.
 * @param cv The condition that will be signaled from another thread.
 */
void wait(std::condition_variable& cv)
{
    std::mutex doneMutex;
    std::unique_lock<std::mutex> lock(doneMutex);
    cv.wait(lock);
}

/*
  This process adds all values until a zero is passed as value.
  Then it will yield the result and start over again.
*/

struct adder
{
    int _sum = 0;
    process_state_scheduled _state = await_forever;

    void await(int x) {
        _sum += x;
        if (x == 0) {
            _state = yield_immediate;
        }
    }

    int yield() {
        int result = _sum;
        _sum = 0;
        _state = await_forever;
        return result;
    }

    auto state() const { return _state; }
};


int main()
{
    std::condition_variable doneCV;

    sender<int> send;
    receiver<int> receiver;
    std::tie(send, receiver) = channel<int>(default_executor);

    std::atomic_bool done{false};
    int launched = 0;

    auto calculator = receiver |
                      adder{} |
                      [&_done = done, &doneCV](int x) {
                          std::cout << x << '\n';
                          _done = true;
                          doneCV.notify_one();
                      };

    receiver.set_ready();

    auto start = std::chrono::steady_clock::now();
    send(1);
    send(2);
    send(3);
    launched += 3;
    for(int i = 1; i < 65536 * 16; ++i)
    {
        send(i);
        ++launched;
    }
    send(0);
    ++launched;
    auto end_launch = std::chrono::steady_clock::now();

    std::cerr << "done: " << done << ", launched: " << launched << std::endl;
    wait(doneCV);

    auto end_wait = std::chrono::steady_clock::now();
    std::cout << "Launch took " << std::chrono::duration <double, std::milli> (end_launch - start).count()
              << ". Wait took " << std::chrono::duration <double, std::milli> (end_wait - end_launch).count()
              << "\nExiting" << std::endl;
}