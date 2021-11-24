#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <iostream>
#include "orderbook.h"
#include "fill_allocator.h"
#include "trade_event_handlers.h"
#include "market.h"

struct PriceAndQuantity {
	Price price;
	Quantity quantity;
};

struct OrderMaker {
	TimeStamp timestamp = 1;
	unsigned long long id_number = 1;
	Order MakeOrder(const Price price, const Quantity quantity) {
		char id_string[32] = { 0 };
		snprintf(id_string, sizeof(id_string), "%llu", id_number++);
		return { price, quantity, { id_string, timestamp++ } };
	}
};

// For computing the total quantity of one side at a specific price level.
// Used in verifying partial fills.
template<typename PrioritySortedOrders>
Quantity TotalQuantity(const PrioritySortedOrders& orders) {
	Quantity total_quantity = 0;
	for (const auto& [key, quantity] : orders) {
		total_quantity += quantity;
	}
	return total_quantity;
}

// We want to track fill history to verify matching engine's actions.
struct TradeEvent {
	Side side;
	Price matched_price;
	Quantity matched_quantity;
	Order aggressor_order;
	PriorityKey opposite_side_key;
};
using TradeEvents = std::vector<TradeEvent>;

// For accumulating an instrument's fills from the matching engine, then checking against a reference result.
// For example, to check whether the fills are FIFO as expected.
struct TradeEventAccumulator {
	TradeEvents trade_event_history;
	void HandleTradeEvent(const Side side, const Price matched_price, const Quantity matched_quantity, const Order& aggressor_order, const PriorityKey& opposite_side_key) {
		trade_event_history.push_back({ side, matched_price, matched_quantity, aggressor_order, opposite_side_key });
	}
	bool WereFillsFIFO() const {
		TimeStamp last_time_stamp = 0;
		size_t i = 0;
		for (const auto& trade_event : trade_event_history) {
			if (i > 0) {
				if (last_time_stamp > trade_event.opposite_side_key.timestamp) {
					return false;
				}
			}
			last_time_stamp = trade_event.opposite_side_key.timestamp;
			++i;
		}
		return true;
	}
};

// For gathering all the orders of the whole market at any one time.
struct FullOrderDetailHandler {
	std::map<Id, FullOrderDetail> orders;
	void HandleFullOrderDetail(const FullOrderDetail& full_order_detail) {
		orders[full_order_detail.order.key.id] = full_order_detail;
	}
	bool operator==(const FullOrderDetailHandler& rhs) const {
		auto it_lhs = orders.begin(), it_rhs = rhs.orders.begin();
		for (; (it_lhs != orders.end()) && (it_rhs != rhs.orders.end()); ++it_lhs, ++it_rhs) {
			if (it_lhs->first != it_rhs->first) return false;
			if (it_lhs->second != it_rhs->second) return false;
		}
		if ((it_lhs != orders.end()) || (it_rhs != rhs.orders.end())) return false;
		return true;
	}
	bool operator!=(const FullOrderDetailHandler& rhs) const {
		return !((*this) == rhs);
	}
};

SCENARIO("FIFO prioritiser and greedy allocator are used for filling orders in FIFO manner", "[matcher]") {
	GIVEN("zero or more resting orders on the opposite side") {
		GreedyFillAllocator matcher;
		Side side = Side::Buy;
		const Price matched_price = 999;
		using PrioritySortedOrders = std::map<PriorityKey, Quantity, PriorityKey::TimeStampComparator>;
		PrioritySortedOrders opposite_side_resting_orders;
		opposite_side_resting_orders[{"1", 1 }] = 10;
		opposite_side_resting_orders[{ "2", 2 }] = 20;
		opposite_side_resting_orders[{ "3", 3 }] = 30;
		Quantity total_available_opposite_side_quantity = TotalQuantity(opposite_side_resting_orders);
		OrderMaker order_maker;
		TradeEventAccumulator trade_event_accumulator;

		WHEN("there are no resting orders on the opposite side") {
			opposite_side_resting_orders.clear();
			const Quantity order_quantity = 123;
			Order aggressor_order = order_maker.MakeOrder(matched_price, order_quantity);
			const Order original_aggressor_order = aggressor_order;
			
			matcher.Fill(side, matched_price, aggressor_order, opposite_side_resting_orders, trade_event_accumulator);

			THEN("aggressor is not filled, resting orders remain empty") {
				REQUIRE(aggressor_order.quantity == order_quantity);
				REQUIRE(opposite_side_resting_orders.empty());
			}
			THEN("aggressor remains unchanged") {
				REQUIRE(original_aggressor_order == aggressor_order);
			}
		}
		WHEN("there are only enough resting orders to partially fill aggressor") {
			const Quantity order_quantity = total_available_opposite_side_quantity + 123;
			Order aggressor_order = order_maker.MakeOrder(matched_price, order_quantity);
			const Order original_aggressor_order = aggressor_order;
			
			matcher.Fill(side, matched_price, aggressor_order, opposite_side_resting_orders, trade_event_accumulator);

			THEN("aggressor is partially filled, consuming all resting orders on the opposite side") {
				REQUIRE(aggressor_order.quantity == order_quantity - total_available_opposite_side_quantity);
				REQUIRE(TotalQuantity(opposite_side_resting_orders) == 0);
				REQUIRE(opposite_side_resting_orders.empty());
			}
			THEN("only aggressor's quantity field is modified after the fill") {
				Order aggressor_order_after_fill = aggressor_order;
				REQUIRE(original_aggressor_order != aggressor_order_after_fill);
				
				aggressor_order_after_fill.quantity = original_aggressor_order.quantity;
				REQUIRE(original_aggressor_order == aggressor_order_after_fill);
			}
			THEN("fills are FIFO") {
				REQUIRE(trade_event_accumulator.WereFillsFIFO());
			}
		}
		WHEN("there is exactly enough resting orders to fully fill aggressor") {
			const Quantity order_quantity = total_available_opposite_side_quantity;
			Order aggressor_order = order_maker.MakeOrder(matched_price, order_quantity);
			const Order original_aggressor_order = aggressor_order;

			matcher.Fill(side, matched_price, aggressor_order, opposite_side_resting_orders, trade_event_accumulator);

			THEN("aggressor is fully filled, consuming all resting orders on the opposite side") {
				REQUIRE(aggressor_order.quantity == 0);
				REQUIRE(opposite_side_resting_orders.empty());
			}
			THEN("only aggressor's quantity field is modified after the fill") {
				Order aggressor_order_after_fill = aggressor_order;
				REQUIRE(original_aggressor_order != aggressor_order_after_fill);

				aggressor_order_after_fill.quantity = original_aggressor_order.quantity;
				REQUIRE(original_aggressor_order == aggressor_order_after_fill);
			}
			THEN("fills are FIFO") {
				REQUIRE(trade_event_accumulator.WereFillsFIFO());
			}
		}
		WHEN("there os more than enough resting orders to fully fill aggressor") {
			const Quantity order_quantity = total_available_opposite_side_quantity - 1;
			Order aggressor_order = order_maker.MakeOrder(matched_price, order_quantity);
			const Order original_aggressor_order = aggressor_order;

			matcher.Fill(side, matched_price, aggressor_order, opposite_side_resting_orders, trade_event_accumulator);

			THEN("aggressor is fully filled, with some resting orders remaining on the opposite side") {
				REQUIRE(aggressor_order.quantity == 0);
				REQUIRE(TotalQuantity(opposite_side_resting_orders) == total_available_opposite_side_quantity - order_quantity);
				REQUIRE(!opposite_side_resting_orders.empty());
			}
			THEN("only aggressor's quantity field is modified after the fill") {
				Order aggressor_order_after_fill = aggressor_order;
				REQUIRE(original_aggressor_order != aggressor_order_after_fill);

				aggressor_order_after_fill.quantity = original_aggressor_order.quantity;
				REQUIRE(original_aggressor_order == aggressor_order_after_fill);
			}
			THEN("fills are FIFO") {
				REQUIRE(trade_event_accumulator.WereFillsFIFO());
			}
		}
		WHEN("the aggressor quantity is zero") {
			const Quantity order_quantity = 0;
			Order aggressor_order = order_maker.MakeOrder(matched_price, order_quantity);
			const Order original_aggressor_order = aggressor_order;

			matcher.Fill(side, matched_price, aggressor_order, opposite_side_resting_orders, trade_event_accumulator);
			
			THEN("aggressor and resting orders are unmodified") {
				REQUIRE(aggressor_order.quantity == 0);
				REQUIRE(TotalQuantity(opposite_side_resting_orders) == total_available_opposite_side_quantity);
				REQUIRE(!opposite_side_resting_orders.empty());
				REQUIRE(original_aggressor_order == aggressor_order);
			}
		}
	}
}

SCENARIO("Market has orders", "[market]") {
	GIVEN("a market initially with only buys") {
		OrderMaker order_maker;
		GreedyFillAllocator fill_allocator;
		TradeEventAccumulator trade_event_accumulator;
		Market<PriorityKey::TimeStampComparator, GreedyFillAllocator, TradeEventAccumulator> market(fill_allocator, trade_event_accumulator);
		FullOrderDetailHandler full_order_details_handler;
		const PriceAndQuantity orders[] = {
			{ 10002, 1 },
			{ 10002, 2 },
			{ 10004, 3 },
			{ 10005, 4 },
			{ 10006, 5 },
			{ 10006, 6 },
			{ 10008, 7 },
		};
		for (const auto& order : orders) {
			Order aggressor_order = order_maker.MakeOrder(order.price, order.quantity);
			REQUIRE(FillExtent::None == market.Buy("ABC", aggressor_order));
		}

		WHEN("there are no sells") {
			const Quantity order_quantity = 1000;
			Order aggressor_order = order_maker.MakeOrder(10008, order_quantity);
			const FillExtent fill_extent = market.Buy("ABC", aggressor_order);
			THEN("no fills are performed") {
				REQUIRE(FillExtent::None == fill_extent);
				REQUIRE(order_quantity == aggressor_order.quantity);
			}
			THEN("final state of all orderbooks are correct") {
				market.ForEachOrderByTime(full_order_details_handler);
				const std::map<Id, FullOrderDetail> expected_orders = {
					{"1", { Side::Buy, "ABC", { 10002, 1, { "1", 1 } } } },
					{"2", { Side::Buy, "ABC", { 10002, 2, { "2", 2 } } } },
					{"3", { Side::Buy, "ABC", { 10004, 3, { "3", 3 } } } },
					{"4", { Side::Buy, "ABC", { 10005, 4, { "4", 4 } } } },
					{"5", { Side::Buy, "ABC", { 10006, 5, { "5", 5 } } } },
					{"6", { Side::Buy, "ABC", { 10006, 6, { "6", 6 } } } },
					{"7", { Side::Buy, "ABC", { 10008, 7, { "7", 7 } } } },
					{"8", { Side::Buy, "ABC", { 10008, order_quantity, { "8", 8 } } } },
				};
				REQUIRE(full_order_details_handler.orders == expected_orders);
			}
		}
		WHEN("there are sells, but for another instrument") {
			const Price order_price = 10005;
			const Quantity order_quantity = 25;
			Order aggressor_order = order_maker.MakeOrder(order_price, order_quantity);
			const FillExtent fill_extent = market.Sell("DEF", aggressor_order);
			THEN("no fills are performed") {
				REQUIRE(FillExtent::None == fill_extent);
				REQUIRE(order_quantity == aggressor_order.quantity);
			}
			THEN("final state of all orderbooks are correct") {
				market.ForEachOrderByTime(full_order_details_handler);
				const std::map<Id, FullOrderDetail> expected_orders = {
					{"1", { Side::Buy, "ABC", { 10002, 1, { "1", 1 } } } },
					{"2", { Side::Buy, "ABC", { 10002, 2, { "2", 2 } } } },
					{"3", { Side::Buy, "ABC", { 10004, 3, { "3", 3 } } } },
					{"4", { Side::Buy, "ABC", { 10005, 4, { "4", 4 } } } },
					{"5", { Side::Buy, "ABC", { 10006, 5, { "5", 5 } } } },
					{"6", { Side::Buy, "ABC", { 10006, 6, { "6", 6 } } } },
					{"7", { Side::Buy, "ABC", { 10008, 7, { "7", 7 } } } },
					{"8", { Side::Sell, "DEF", { order_price, order_quantity, { "8", 8 } } } },
				};
				REQUIRE(full_order_details_handler.orders == expected_orders);
			}
		}
		WHEN("the lowest ask is above the highest bid") {
			const Price order_price = 10009;
			const Quantity order_quantity = 1;
			Order aggressor_order = order_maker.MakeOrder(order_price, order_quantity);
			const FillExtent fill_extent = market.Sell("ABC", aggressor_order);
			THEN("no fills are performed") {
				REQUIRE(FillExtent::None == fill_extent);
				REQUIRE(order_quantity == aggressor_order.quantity);
			}
			THEN("final state of all orderbooks are correct") {
				market.ForEachOrderByTime(full_order_details_handler);
				const std::map<Id, FullOrderDetail> expected_orders = {
					{"1", { Side::Buy, "ABC", { 10002, 1, { "1", 1 } } } },
					{"2", { Side::Buy, "ABC", { 10002, 2, { "2", 2 } } } },
					{"3", { Side::Buy, "ABC", { 10004, 3, { "3", 3 } } } },
					{"4", { Side::Buy, "ABC", { 10005, 4, { "4", 4 } } } },
					{"5", { Side::Buy, "ABC", { 10006, 5, { "5", 5 } } } },
					{"6", { Side::Buy, "ABC", { 10006, 6, { "6", 6 } } } },
					{"7", { Side::Buy, "ABC", { 10008, 7, { "7", 7 } } } },
					{"8", { Side::Sell, "ABC", { order_price, order_quantity, { "8", 8 } } } },
				};
				REQUIRE(full_order_details_handler.orders == expected_orders);
			}
		}
		WHEN("partial fills are possible") {
			const Price order_price = 10005;
			const Quantity order_quantity = 25;
			Order aggressor_order = order_maker.MakeOrder(order_price, order_quantity);
			const FillExtent fill_extent = market.Sell("ABC", aggressor_order);
			const Quantity expected_unfillled_quantity = order_quantity - (4 + 5 + 6 + 7);
			THEN("partial fills are performed") {
				REQUIRE(FillExtent::Partial == fill_extent);
				REQUIRE(expected_unfillled_quantity == aggressor_order.quantity);
			}
			THEN("final state of all orderbooks are correct") {
				market.ForEachOrderByTime(full_order_details_handler);
				const std::map<Id, FullOrderDetail> expected_orders = {
					{"1", { Side::Buy, "ABC", { 10002, 1, { "1", 1 } } } },
					{"2", { Side::Buy, "ABC", { 10002, 2, { "2", 2 } } } },
					{"3", { Side::Buy, "ABC", { 10004, 3, { "3", 3 } } } },
					/*
					{"4", { Side::Buy, "ABC", { 10005, 4, { "4", 4 } } } },
					{"5", { Side::Buy, "ABC", { 10006, 5, { "5", 5 } } } },
					{"6", { Side::Buy, "ABC", { 10006, 6, { "6", 6 } } } },
					{"7", { Side::Buy, "ABC", { 10008, 7, { "7", 7 } } } },
					*/
					{"8", { Side::Sell, "ABC", { order_price, expected_unfillled_quantity, { "8", 8 } } } },
				};
				REQUIRE(full_order_details_handler.orders == expected_orders);
			}
		}
		WHEN("complete fills are possible") {
			Order aggressor_order = order_maker.MakeOrder(10005, 10);
			const FillExtent fill_extent = market.Sell("ABC", aggressor_order);
			THEN("complete fills are performed") {
				REQUIRE(FillExtent::Full == fill_extent);
				REQUIRE(0 == aggressor_order.quantity);
			}
			THEN("final state of all orderbooks are correct") {
				market.ForEachOrderByTime(full_order_details_handler);
				const std::map<Id, FullOrderDetail> expected_orders = {
					{"1", { Side::Buy, "ABC", { 10002, 1, { "1", 1 } } } },
					{"2", { Side::Buy, "ABC", { 10002, 2, { "2", 2 } } } },
					{"3", { Side::Buy, "ABC", { 10004, 3, { "3", 3 } } } },
					{"4", { Side::Buy, "ABC", { 10005, 4, { "4", 4 } } } },
					{"5", { Side::Buy, "ABC", { 10006, 5 - 3, { "5", 5 } } } },
					{"6", { Side::Buy, "ABC", { 10006, 6, { "6", 6 } } } },
					/*
					{"7", { Side::Buy, "ABC", { 10008, 7, { "7", 7 } } } },
					*/
				};
				REQUIRE(full_order_details_handler.orders == expected_orders);
			}
		}
	}
}
