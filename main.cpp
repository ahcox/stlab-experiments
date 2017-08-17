#include <iostream>
#include "stlab/concurrency/default_executor.hpp"
#include "stlab/concurrency/future.hpp"
#include "stlab/concurrency/channel.hpp"
#include "stlab/concurrency/concurrency.hpp"

//using namespace std;
//using namespace stlab;
//
//int main() {
//    std::cout << "Hello, World!" << std::endl;
//    //constexpr NUM_ITERS
//
//    auto f = async(default_executor, [] { return 42; });
//
//    // Waiting just for illustrational purpose
//    while (!f.get_try()) { this_thread::sleep_for(chrono::milliseconds(1)); }
//
//    cout << "The answer is " << f.get_try().value() << "\n";
//
//    return 0;
//}

#include <atomic>
#include <iostream>
#include <thread>

#include <stlab/concurrency/channel.hpp>
#include <stlab/concurrency/default_executor.hpp>

using namespace std;
using namespace stlab;

void channel() {
    sender<int> send;
    receiver<int> receive;

    tie(send, receive) = channel<int>(default_executor);

    std::atomic<int> done { 0 };

    auto hold = receive | [&_done = done](int x) {
        this_thread::sleep_for(chrono::milliseconds((2-x)*5000));
        cout << x << '\n';
        cout.flush();
        ++_done;
    };

    // It is necessary to mark the receiver side as ready, when all connections are
    // established
    receive.set_ready();

    send(1);
    send(2);
    send(3);

    // Waiting just for illustrational purpose
    while (done.load() < 3) {
        this_thread::sleep_for(chrono::milliseconds(1));
    }
}

int main() {
    channel();
    return 0;
}