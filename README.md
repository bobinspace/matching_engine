## How to build and run the application
`./run.sh` builds and runs `build/src/me_app`, which waits for orders from stdin.
- Take orders from console: `./run.sh`.
- Take orders from piped input: `cat sample_input.txt | ./run.sh`

## How to build and run tests
`./test.sh` builds and runs `build/test/me_test`, which runs catch2 unit tests on the matching engine.

## How I approached the problem
- First, understand the requirements.

### Deciding on data structures
- The matching engine needs to match by price, then by timestamp of orders
- The most natural fit for this is to have a std::map with price as key.
- What about the value of the std::map? Since we want FIFO, a queue-like container will be the most natural, since we will be dealing with just the "ends" of the container, popping off the oldest orders first.
- In terms of actual implementation, a std::queue, or better yet, a std::deque without the burden of being an adapted container like std::queue, would serve very well. But in real life, this has limitations:
- In real life, we need to consider 2 aspects of the matching engine:
1) It needs to PRIORITISE which orders to fill first
2) It needs to ALLOCATE how much to consume from each resting order
Consider the prioritisation requirement: If we were to use a std::deque, we would be pretty much stuck with a FIFO/LIFO prioritisation scheme, as element updates within the deque will be expensive. Therefore, a map would be more pragmatic because it allows us to plug in our own custom comparators, aka define our own prioritisation scheme.
(As for the "allocation" requirement, we will discuss in the next section.)
- As for the collection of all order books in the market, this is straightforward: std::map with instrument as key and orderbook as value.

### Extensibility
- I deliberately do not assume price/quantity to be specific native types. Especially for price types, it is not rare for projects to discover one day, that a price/quantity data type is inadequate and the team has to go through some kind of migration exercise to upgrade the type. For this reason, all domain-fundamental types (e.g. price/quantity) are type-aliased. All interfaces taking domain-fundamental types do not take native types but type aliases.
- A real matching engine is likely going to support more than one allocation method. For this reason, the allocation logic is separated into data types (under `fill_allocator.h`) that can be plugged in to the `Orderbook` class:
```
template<typename MatchingOrdersComparator, typename FillAllocator, typename TradeEventHandler>
class Orderbook
```
- Handling of trade events (`TradeEventHandler`) is also separated out into individual data types that can be plugged into the fill allocator:
```
struct GreedyFillAllocator {
	template<typename TradeEventHandler>
	void Fill(const Side side, ...)
```
- In general, I favour "handlers" to be separated out into their own data types so that they are pluggable into templates. Templates are favoured in this exercise, as opposed to inheriting interfaces from abstract classes, for performance reasons.

## Time spent (hours)
Most of the time in the "design" category was away from the keyboard or not involved in any direct coding.
Refactoring/cleanup also formed the most of the implementation times.
- Design of orderbook: 2
- Implementation of orderbook: 5
- Design and implementation of tests: 5
- Project layout and build system: 5