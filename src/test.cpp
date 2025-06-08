#include <cstdint>
#include <map>

using Price = std::uint64_t;
using Volume = std::uint64_t;

enum class Excecution
{
    FILLED,
    PARTIALLY_FILLED,
    CANCELLED
};

enum Side
{
    Buy,
    Sell
};

class LimitOrderBook
{
    std::map<Price, Volume, std::greater<Price>> bids;
    std::map<Price, Volume, std::less<Price>> asks;

    void AddOrder(Side side, Price level, Volume volume);

private:
    template <typename T>
    T::iterator mAddOrder(T &levels, Price price, Volume volume);

    template <typename T>
    void mDeleteOrder(typename T::iterator it, T &levels, Price price,
                      Volume volume);
};

inline void LimitOrderBook::AddOrder(Side side, Price price, Volume volume)
{
    if (side == Side::Buy)
    {
        mAddOrder(bids, price, volume);
    }
    else
    {
        mAddOrder(asks, price, volume);
    }
}

template <typename T>
T::iterator LimitOrderBook::mAddOrder(T &levels, Price price, Volume volume)
{
    auto [it, inserted] = levels.try_emplace(price, volume);
    if (inserted == false)
        it->second += volume;
    return it;
}

template <typename T>
void LimitOrderBook::mDeleteOrder(typename T::iterator it, T &levels,
                                  Price price, Volume volume)
{
    it->second -= volume;
    if (it->volume <= 0)
        levels.erase(it);
}