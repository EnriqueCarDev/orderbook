#include <algorithm>
#include <functional>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <unordered_map>
#include <vector>
enum class Side { Buy, Sell };
enum class OrderType { FillAndKill, GoodTillCancel };

using Quantity = std::int32_t;
using Price = std::int32_t;
using OrderId = std::int32_t;

struct LevelInfo {
  Price price;
  Quantity quantity;
};

using LevelInfos = std::vector<LevelInfo>;

class OrderBookLevelInfos {
 public:
  OrderBookLevelInfos(LevelInfos& bidsInfos, LevelInfos& asksInfos)
      : bids_{bidsInfos}, asks_{asksInfos} {};

  LevelInfos GetBidsLevelInfos() const { return bids_; };
  LevelInfos GetAsksLevelInfos() const { return asks_; };

 private:
  LevelInfos bids_;
  LevelInfos asks_;
};

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

class OrderModify {
 public:
  OrderModify(OrderId orderId, Side side, Quantity quantity, Price price)
      : orderId_{orderId}, side_{side}, quantity_{quantity}, price_{price} {}

  OrderId GetOrderId() const { return orderId_; };
  Side GetOrderSide() const { return side_; };
  Quantity GetOrderQuantity() const { return quantity_; };
  Price GetOrderPrice() const { return price_; };

  OrderPointer ToOrderPointer(OrderType orderType) const {
    return std::make_shared<Order>(GetOrderId(), orderType, GetOrderQuantity(),
                                   GetOrderPrice(), GetOrderSide());
  }

 private:
  OrderId orderId_;
  Side side_;
  Quantity quantity_;
  Price price_;
};

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

      const auto& [bestAsks, _] = *asks_.begin();
      return price >= bestAsks;
    } else {
      if(bids_.empty()) return false;

      const auto& [bestBids, _] = *bids_.begin();
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
        Quantity quantity =
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
        if(order->GetOrderType() == OrderType::FillAndKill) {
          CancelOrder(order->GetOrderId());
        }
      }

      if(!asks.empty()) {
        auto& [_, asks] = *asks_.begin();
        auto& order = asks.front();
        orders_.erase(order->GetOrderId());
        CancelOrder(order->GetOrderId());
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

    const auto& [order, location] = orders_.at(orderId);

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

  Trades ModifyOrder(OrderModify order) {
    if(!orders_.contains(order.GetOrderId())) {
      return {};
    }

    const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
    CancelOrder(order.GetOrderId());
    AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));

    return MatchOrders();
  }

  OrderBookLevelInfos GetLevelInfos() const {
    LevelInfos bidsInfos, asksInfos;
    bidsInfos.reserve(orders_.size());
    asksInfos.reserve(orders_.size());

    auto CreateLevelInfos = [](Price price, const OrderPointers& orders) {
      return LevelInfo{
          price,
          std::accumulate(orders.begin(), orders.end(), (Quantity)0,
                          [](Quantity runningSum, const OrderPointer& order) {
                            return runningSum + order->GetRemainingQuantity();
                          })};
    };

    for(const auto& [price, orders] : bids_) {
      bidsInfos.push_back(CreateLevelInfos(price, orders));
    }

    for(const auto& [price, orders] : asks_) {
      asksInfos.push_back(CreateLevelInfos(price, orders));
    }

    return OrderBookLevelInfos{bidsInfos, asksInfos};
  }
};

int main() { return 0; }