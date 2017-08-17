//
// Created by Andrew Cox on 14/08/2017.
// Copied from http://stlab.cc/libraries/concurrency/future/async.html
// and then modified.

#include <iostream>
#include <thread>

#include <stlab/concurrency/default_executor.hpp>
#include <stlab/concurrency/future.hpp>

using namespace std;
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

void simpleAsync()
{
    std::condition_variable doneCV;
    int acc = 0;
    for(int i = 0; i < 10; ++i) {
        cerr << "Iteration " << i << endl;
        auto f = async(default_executor, [&doneCV] {
            //this_thread::sleep_for(chrono::milliseconds(1 + int(5.0 * rand() / RAND_MAX)));
            doneCV.notify_all();
            return 42;
        });//}).then([&doneCV](int i){ doneCV.notify_one(); this_thread::sleep_for(chrono::milliseconds(3000)); return i; });

        // Waiting just for illustrative purposes
        wait(doneCV);
        // Still need to spin due to race with async task:
        while (!f.get_try()) { this_thread::sleep_for(chrono::microseconds(1)); cerr << ".";}
        acc += f.get_try().value();
        cerr << endl;
    }
    cout << "The answer is " << acc << "\n";
}


template <typename E, typename F, typename ...Args>
auto asyncWrapper(E executor, F&& f, Args&&... args)
-> future<std::result_of_t<F (Args...)>>
{
    return stlab::async(executor, std::forward<F>(f), std::forward<Args>(args)...);
}

void passingAsync2()
{
    auto f = asyncWrapper(default_executor, [] (int i, int j) {
        double slow = 1.000000000000000000001;
        for(int k = 100000; k > j; --k){
            slow *= slow;
        }
        return (i + j + 42) * slow;
    }, 33, 25);

    // Waiting
    int yields = 0;
    while (!f.get_try()) { this_thread::yield(); ++yields; } //sleep_for(chrono::microseconds(1)); cerr << ".";}

    auto val = f.get_try().value();
    cout << "The answer is " << val << ", (yields = " << yields << ")\n";
}

void passingAsync1()
{
    std::condition_variable doneCV;
    //auto f = async(default_executor, [&doneCV] {
    auto f = asyncWrapper(default_executor, [&doneCV] {
        //this_thread::sleep_for(chrono::milliseconds(1 + int(5.0 * rand() / RAND_MAX)));
        doneCV.notify_all();
        return 42;
    });

    // Waiting just for illustrative purposes
    wait(doneCV);
    // Still need to spin due to race with async task:
    while (!f.get_try()) { this_thread::sleep_for(chrono::microseconds(1)); cerr << ".";}

    auto val = f.get_try().value();
    cerr << endl;
    cout << "The answer is " << val << "\n";
}

int main()
{
    //simpleAsync();
    passingAsync1();
    passingAsync2();

}