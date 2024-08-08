#pragma once
#include <eosio/time.hpp>
#include <eosio/print.hpp>
#include <eosevm/block_mapping.hpp>

#define VALUE_PROMOTER_PENDING(T)\
    struct T##_pending {\
        T value;\
        time_point time;\
        bool is_active(time_point_sec genesis_time, time_point current_time)const {\
            eosevm::block_mapping bm(genesis_time.sec_since_epoch());\
            auto current_block_num = bm.timestamp_to_evm_block_num(current_time.time_since_epoch().count());\
            auto pending_block_num = bm.timestamp_to_evm_block_num(time.time_since_epoch().count());\
            return current_block_num > pending_block_num;\
        }\
    };

#define VALUE_PROMOTER_IMPL(T)\
    T get_value(time_point_sec genesis_time, time_point current_time)const {\
        T current_value = cached_value;\
        if(pending_value.has_value() && pending_value->is_active(genesis_time, current_time)) {\
            current_value = pending_value->value;\
        }\
        return current_value;\
    }\
    std::pair<T, bool> get_value_and_maybe_promote(time_point_sec genesis_time, time_point current_time) {\
        T current_value = cached_value;\
        bool promoted = false;\
        if(pending_value.has_value() && pending_value->is_active(genesis_time, current_time)) {\
            current_value = pending_value->value;\
            promote_pending();\
            promoted = true;\
        }\
        return std::pair<T, bool>(current_value, promoted);\
    }\
    template <typename Visitor>\
    void update(Visitor&& visitor_fn, time_point_sec genesis_time, time_point current_time) {\
        auto value = get_value_and_maybe_promote(genesis_time, current_time);\
        visitor_fn(value.first);\
        pending_value.emplace(T##_pending{\
            .value = value.first,\
            .time = current_time\
        });\
    }\
    void promote_pending() {\
        eosio::check(pending_value.has_value(), "no pending value");\
        cached_value = pending_value.value().value;\
        pending_value.reset();\
    }

#define VALUE_PROMOTER(T)\
VALUE_PROMOTER_PENDING(T);\
struct value_promoter_##T {\
    std::optional<T##_pending> pending_value;\
    T cached_value = T{};\
    VALUE_PROMOTER_IMPL(T)\
};

#define VALUE_PROMOTER_REV(T)\
VALUE_PROMOTER_PENDING(T);\
struct value_promoter_##T {\
    T cached_value = T{};\
    std::optional<T##_pending> pending_value;\
    VALUE_PROMOTER_IMPL(T)\
};
