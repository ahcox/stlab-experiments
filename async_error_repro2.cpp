//
// Created by Andrew Cox 2017.
//

#include <iostream>
#include <future>

#include <stlab/concurrency/default_executor.hpp>
#include <stlab/concurrency/future.hpp>

struct SharedState
{
    int counter1;
};

void spawn(const SharedState& in, SharedState& inOut)
{
    // stlab::async won't accept this:
    auto taskNonConst = [] (int i, long double f, SharedState& inOut) -> SharedState&
    {
        inOut.counter1 += i * f;
        return inOut;
    };

    // stlab::async won't accept this:
    auto taskNonConst_02_no_return = [] (int i, long double f, SharedState& inOut)
    {
        inOut.counter1 += i * f;
    };

    auto taskConst = [] (int i, long double f, const SharedState& in, SharedState& inOut)
    {
        inOut.counter1 += i * f * in.counter1;
    };

    const int i = rand() & 0xff;
    const long double f = rand() / double(RAND_MAX);

    // Direct calls work fine:
    auto& direct = taskNonConst(i, f, inOut);
    taskNonConst_02_no_return(i, f, inOut);
    taskConst(i, f, in, inOut);

    // Non-const references fail to compile, even using std::ref():

    auto stdFuture1 = std::async(std::launch::async, taskNonConst, i, f, std::ref(inOut)); // Compiles [expect: compile as std::ref used]
    //auto stdFuture2 = std::async(std::launch::async, taskNonConst, i, f, inOut); // Fails [expect: fail as non-const ref not allowed for async]
    auto stlabFuture1 = stlab::async(stlab::default_executor, taskNonConst, i, f, std::ref(inOut)); // compiles [expect: compile as std::ref used]
    //auto stlabFuture2 = stlab::async(stlab::default_executor, taskNonConst, i, f, inOut);  // Fails [expect: fail as non-const ref not allowed]

    auto stdFuture3 = std::async(std::launch::async, taskNonConst_02_no_return, i, f, std::ref(inOut));
    auto stlabFuture3 = stlab::async(stlab::default_executor, taskNonConst_02_no_return, i, f, std::ref(inOut)); //compiles

    auto stdFuture4 = std::async(std::launch::async, taskConst, i, f, std::cref(in), std::ref(inOut));
    auto stlabFuture4 = stlab::async(stlab::default_executor, taskConst, i, f, std::cref(in), std::ref(inOut)); // compiles

    while(!stlabFuture4.get_try()) {
        std::this_thread::yield();
    }
}

int main()
{
    SharedState in { 1 };
    SharedState inOut { 1 };

    spawn(in, inOut);

    // Prevent dead code elimination:
    auto result = inOut.counter1;
    std::cout << "result = " << result << std::endl;

    return result <= 0.0;
}