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
    long double weighted1;
    std::string tag; // < make it not POD.
};

void spawn(const SharedState& in, SharedState& out, SharedState& inOut)
{
    // stlab::async will accept this (refs are to const):
    auto taskPureConst = [] (int i, long double f, const SharedState& in, const SharedState& in2) -> float
    {
        long double acc = f;
        for(int j = 0; j < i; ++j)
        {
            acc += in.weighted1 + in2.weighted1;
        }

        return acc / (2 * i);
    };

    // stlab::async won't accept this:
    auto taskNonConst = [] (int i, long double f, SharedState& inOut) -> SharedState&
    {
        for(int j = 0; j < i; ++j)
        {
            inOut.weighted1 += f;
        }
        inOut.counter1 += i;

        return inOut;
    };

    // stlab::async won't accept this:
    auto taskNonConst_02_no_return = [] (int i, long double f, SharedState& inOut){
        for(int j = 0; j < i; ++j)
        {
            inOut.weighted1 += f;
        }
        inOut.counter1 += i;
    };

    auto taskNonConst_03_pass_through_capture_no_return = [&inOut] (int i, long double f){
        for(int j = 0; j < i; ++j)
        {
            inOut.weighted1 += f;
        }
        inOut.counter1 += i;
    };

    // stlab::async won't accept this:
    auto taskConst = [] (int i, long double f, const SharedState& in, SharedState& out) -> const SharedState &
    {
        out.counter1 = in.counter1;
        out.weighted1 = in.weighted1;
        for(int j = 0; j < i; ++j)
        {
            out.weighted1 += f;
        }
        out.counter1 += i;

        return in;
    };

    const int i = rand() & 0xff;
    const long double f = rand() / double(RAND_MAX);

    // Direct calls work fine:
    auto& direct = taskNonConst(i, f, inOut);
    taskNonConst_02_no_return(i, f, inOut);
    taskNonConst_03_pass_through_capture_no_return(i, f);
    const SharedState& directConst = taskConst(i, f, in, out);
    std::cout << "taskPureConst(): "  << taskPureConst(i,f, in, inOut) << std::endl;

    // Const references compile fine if async allowed to copy them:
    std::async(std::launch::async, taskPureConst, i,f, in, inOut);
    stlab::async(stlab::default_executor, taskPureConst, i,f, std::cref(in), std::cref(inOut));

    // Const references fail to compile if forced to be references and not copied:
    std::async(std::launch::async, taskPureConst, i,f, std::cref(in), std::cref(inOut));
    stlab::async(stlab::default_executor, taskPureConst, i,f, std::cref(in), std::cref(inOut));

    // Non-const references known at lambda creation time can be "passed" as captures:
    auto stdFutureCapture1 = std::async(std::launch::async, taskNonConst_03_pass_through_capture_no_return, i, f);
    //auto stlabFutureCapture1 = stlab::async(stlab::default_executor, taskNonConst_03_pass_through_capture_no_return, i, f); // Compiles

    // Non-const references fail to compile, even using std::ref():

    auto stdFuture1 = std::async(std::launch::async, taskNonConst, i, f, std::ref(inOut)); // Compiles [expect: compile as std::ref used]
    //auto stdFuture2 = std::async(std::launch::async, taskNonConst, i, f, inOut); // Fails [expect: fail as non-const ref not allowed for async]
    //auto stlabFuture1 = stlab::async(stlab::default_executor, taskNonConst, i, f, std::ref(inOut)); // Fail [expect: compile as std::ref used]
    //auto stlabFuture2 = stlab::async(stlab::default_executor, taskNonConst, i, f, inOut);  // Fails [expect: fail as non-const ref not allowed]

    auto stdFutureNoRet1 = std::async(std::launch::async, taskNonConst_02_no_return, i, f, std::ref(inOut));
    //auto stlabFutureNoRet1 = stlab::async(stlab::default_executor, taskNonConst_02_no_return, i, f, std::ref(inOut)); // Fails

    auto stdFutureConst1 = std::async(std::launch::async, taskConst, i, f, in, std::ref(out));
    //auto stlabFutureConst1 = stlab::async(stlab::default_executor, taskConst, i, f, in, std::ref(out)); // Fails

    //while(!stlabFutureCapture1.get_try()) {
    //    std::this_thread::yield();
    //}
}

int main()
{
    SharedState in { 0, 0.0, "Hi"};
    SharedState out { 0, 0.0, "Hi"};
    SharedState inOut { 0, 0.0, "Hi"};

    spawn(in, out, inOut);

    // Prevent dead code elimination:
    auto result = out.counter1 + out.weighted1 + out.tag.capacity() + inOut.counter1 + inOut.weighted1 + inOut.tag.capacity();
    std::cout << "result = " << result << std::endl;

    return result <= 0.0;
}