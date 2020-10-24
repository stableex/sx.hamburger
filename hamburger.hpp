#pragma once

#include <eosio/singleton.hpp>
#include <eosio/asset.hpp>

namespace hamburger {

    using eosio::asset;
    using eosio::extended_symbol;
    using eosio::symbol;
    using eosio::symbol_code;
    using eosio::name;
    using eosio::singleton;
    using eosio::multi_index;
    using eosio::time_point_sec;

    /**
     * Hamburger pairs
     */
    struct [[eosio::table]] pairs_row {
        uint64_t            id;
        symbol_code         code;
        extended_symbol     token0;
        extended_symbol     token1;
        asset               reserve0;
        asset               reserve1;
        uint64_t            total_liquidity;
        uint32_t            last_update_time;
        uint32_t            created_time;

        uint64_t primary_key() const { return id; }
    };
    typedef eosio::multi_index< "pairs"_n, pairs_row > pairs;

    /**
     * Defibox config
     */
    struct [[eosio::table]] global_row {
        uint8_t             contract_status = 1;
        uint8_t             mine_status = 1;
        uint8_t             trade_fee = 20;
        uint8_t             protocol_fee = 10;
    };
    typedef eosio::singleton< "config"_n, global_row > global;

    /**
     * Deposits
     */
    struct [[eosio::table("deposits")]] deposits_row {
        name            owner;
        asset           quantity0;
        asset           quantity1;

        uint64_t primary_key() const { return owner.value; }
    };
    typedef eosio::multi_index< "deposits"_n, deposits_row > deposits;

        /**
     * HBG mine pools
     */
    struct [[eosio::table("pools")]] pools_row {
        uint64_t        pair_id;
        double_t        weight;
        asset           balance;
        asset           issued;
        uint32_t        last_issue_time;
        uint32_t        start_time;
        uint32_t        end_time;

        uint64_t primary_key() const { return pair_id; }
    };
    typedef eosio::multi_index< "pools"_n, pools_row > pools;

    /**
     * ## STATIC `get_fee`
     *
     * Get total fee
     *
     * ### returns
     *
     * - `{uint8_t}` - total fee (trade + protocol)
     *
     * ### example
     *
     * ```c++
     * const uint8_t fee = hamburger::get_fee();
     * // => 30
     * ```
     */
    static uint8_t get_fee()
    {
        hamburger::global _global( "hamburgerswp"_n, "hamburgerswp"_n.value );
        hamburger::global_row global = _global.get_or_default();
        return global.trade_fee + global.protocol_fee;
    }

    /**
     * ## STATIC `get_reserves`
     *
     * Get reserves for a pair
     *
     * ### params
     *
     * - `{uint64_t} pair_id` - pair id
     * - `{symbol} sort` - sort by symbol (reserve0 will be first item in pair)
     *
     * ### returns
     *
     * - `{pair<asset, asset>}` - pair of reserve assets
     *
     * ### example
     *
     * ```c++
     * const uint64_t pair_id = 12;
     * const symbol sort = symbol{"EOS", 4};
     *
     * const auto [reserve0, reserve1] = hamburger::get_reserves( pair_id, sort );
     * // reserve0 => "4585193.1234 EOS"
     * // reserve1 => "12568203.3533 USDT"
     * ```
     */
    static std::pair<asset, asset> get_reserves( const uint64_t pair_id, const symbol sort )
    {
        // table
        hamburger::pairs _pairs( "hamburgerswp"_n, "hamburgerswp"_n.value );
        auto pairs = _pairs.get( pair_id, "HamburgerLibrary: INVALID_PAIR_ID" );
        eosio::check( pairs.reserve0.symbol == sort || pairs.reserve1.symbol == sort, "sort symbol does not match" );

        return sort == pairs.reserve0.symbol ?
            std::pair<asset, asset>{ pairs.reserve0, pairs.reserve1 } :
            std::pair<asset, asset>{ pairs.reserve1, pairs.reserve0 };
    }

    /**
     * ## STATIC `get_rewards`
     *
     * Get rewards for trading
     *
     * ### params
     *
     * - `{uint64_t} pair_id` - pair id
     * - `{asset} from` - tokens we are trading from
     * - `{asset} to` - tokens we are trading to
     *
     * ### returns
     *
     * - {asset} = rewards in BOX
     *
     * ### example
     *
     * ```c++
     * const uint64_t pair_id = 12;
     * const asset from = asset{10000, {"EOS", 4}};
     * const asset to = asset{12345, {"USDT", 4}};
     *
     * const auto rewards = hamburger::get_rewards( pair_id, from, to);
     * // rewards => "0.123456 BOX"
     * ```
     */
    static asset get_rewards( const uint64_t pair_id, asset from, asset to )
    {
        asset res {0, symbol{"HBG",6}};
        auto eos = from.symbol.code().to_string() == "EOS" ? from : to;
        if(eos.symbol.code().to_string() != "EOS")
            return res;     //return 0 if non-EOS pair

        hamburger::pools _pools( "hbgtrademine"_n, "hbgtrademine"_n.value );
        auto poolit = _pools.find( pair_id );
        if(poolit==_pools.end()) return res;
        float newsecs = current_time_point().sec_since_epoch() - poolit->last_issue_time;  //second since last update
        uint64_t newhbg = poolit->weight * 0.005 * newsecs * 1000000;
        auto times = eos.amount / 10000;
        auto total = poolit->balance.amount + newhbg;
        while(times--){
            auto mined = total/10000;   //0.01% of the pool balance
            total -= mined;
            res.amount += mined;
        }
        return res;
    }
}