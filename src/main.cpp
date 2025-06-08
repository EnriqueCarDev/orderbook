#include <algorithm>
#include <functional>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <unordered_map>
#include <vector>

enum class Side { Buy, Sell };
enum class OrderType { FillAndKill, GoodTillCancel };

using Quantity = std::int32_t;
using Price = std::int32_t;
using OrderId = std::int32_t;

class Order {
 public:
  Order(OrderId orderId, OrderType orderType, Quantity quantity, Price price,
        Side side)
      : orderId_{orderId},
        orderType_{orderType},
        initialQuantity_{quantity},
        remainingQuantity_{quantity},
        price_{price},
        side_{side} {}

  OrderId GetOrderId() const { return orderId_; };
  OrderType GetOrderType() const { return orderType_; };
  Quantity GetInitialQuantity() const { return initialQuantity_; };
  Quantity GetRemainingQuantity() const { return remainingQuantity_; };
  Price GetPrice() const { return price_; };
  Side GetSide() const { return side_; };

  bool isFilled() const { return GetRemainingQuantity() == 0; };
  void Fill(Quantity quantity) {
    if(quantity > GetRemainingQuantity()) {
      throw std::logic_error(std::format(
          "Order Id ({}) can't get filled more than its remaining quantity",
          GetOrderId()));
    }
    remainingQuantity_ -= quantity;
  }

 private:
  OrderId orderId_;
  OrderType orderType_;
  Quantity initialQuantity_;
  Quantity remainingQuantity_;
  Price price_;
  Side side_;
};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

struct TradeInfos {
  OrderId orderId;
  Price price;
  Quantity quantity;
};

class Trade {
 public:
  Trade(const TradeInfos& bidTrade, const TradeInfos& askTrade)
      : bidTrade_{bidTrade}, askTrade_{askTrade} {}

 private:
  TradeInfos bidTrade_;
  TradeInfos askTrade_;
};

using Trades = std::vector<Trade>;

class OrderBook {
 private:
  struct OrderEntry {
    OrderPointer order{nullptr};
    OrderPointers::iterator location;
  };

  std::map<Price, OrderPointers, std::greater<Price>> bids_;
  std::map<Price, OrderPointers, std::less<Price>> asks_;
  std::unordered_map<OrderId, OrderEntry> orders_;

  bool CanMatch(Side side, Price price) const {
    if(side == Side::Buy) {
      if(asks_.empty()) return false;

      auto& [bestAsks, _] = *asks_.begin();
      return price >= bestAsks;
    } else {
      if(bids_.empty()) return false;

      auto& [bestBids, _] = *bids_.begin();
      return price <= bestBids;
    }
  };

  Trades MatchOrders() {
    Trades trades;
    trades.reserve(orders_.size());
    while(true) {
      if(asks_.empty() || bids_.empty()) break;

      auto& [bidPrice, bids] = *bids_.begin();
      auto& [askPrice, asks] = *asks_.begin();

      if(bidPrice < askPrice) break;

      while(bids.size() && asks.size()) {
        auto& bid = *bids.begin();
        auto& ask = *asks.begin();
        auto& quantity =
            std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());

        bid->Fill(quantity);
        ask->Fill(quantity);

        if(bid->isFilled()) {
          bids.pop_front();
          orders_.erase(bid->GetOrderId());
        }
        if(ask->isFilled()) {
          asks.pop_front();
          orders_.erase(ask->GetOrderId());
        }

        if(bids.empty()) bids_.erase(bidPrice);
        if(asks.empty()) asks_.erase(askPrice);

        trades.push_back(
            Trade{TradeInfos{bid->GetOrderId(), bid->GetPrice(), quantity},
                  TradeInfos{ask->GetOrderId(), ask->GetPrice(), quantity}});
      }
      if(!bids.empty()) {
        auto& [_, bids] = *bids_.begin();
        auto& order = bids.front();
        orders_.erase(order->GetOrderId());
        // TODO add the GoodTillCancel check
      }

      if(!asks.empty()) {
        auto& [_, asks] = *asks_.begin();
        auto& order = asks.front();
        orders_.erase(order->GetOrderId());
        // TODO add the GoodTillCancel check
      }
    }
    return trades;
  }

 public:
  Trades AddOrder(OrderPointer order) {
    if(orders_.contains(order->GetOrderId())) return {};

    if(!CanMatch(order->GetSide(), order->GetPrice()) &&
       order->GetOrderType() == OrderType::FillAndKill)
      return {};

    OrderPointers::iterator iterator;

    if(order->GetSide() == Side::Buy) {
      auto& orders = bids_[order->GetPrice()];
      orders.push_back(order);
      iterator = std::next(orders.begin(), orders.size() - 1);
    } else {
      auto& orders = asks_[order->GetPrice()];
      orders.push_back(order);
      iterator = std::next(orders.begin(), orders.size() - 1);
    }

    orders_.insert({order->GetOrderId(), OrderEntry{order, iterator}});

    return MatchOrders();
  }

  void CancelOrder(OrderId orderId) {
    if(!orders_.contains(orderId)) return;

    auto& [order, location] = orders_.at(orderId);

    if(order->GetSide() == Side::Buy) {
      auto& orders = bids_[order->GetPrice()];
      orders.erase(location);
      if(orders.empty()) bids_.erase(order->GetPrice());
    } else {
      auto& orders = asks_[order->GetPrice()];
      orders.erase(location);
      if(orders.empty()) asks_.erase(order->GetPrice());
    }

    orders_.erase(order->GetOrderId());
  }
};

int main() { return 0; }