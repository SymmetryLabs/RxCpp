// Copyright (c) Microsoft Open Technologies, Inc. All rights reserved. See License.txt in the project root for license information.

#pragma once

#if !defined(RXCPP_OPERATORS_RX_WINDOW_HPP)
#define RXCPP_OPERATORS_RX_WINDOW_HPP

#include "../rx-includes.hpp"

namespace rxcpp {

namespace operators {

namespace detail {

template<class T>
struct window
{
    typedef typename std::decay<T>::type source_value_type;
    struct window_values
    {
        window_values(int c, int s)
            : count(c)
            , skip(s)
        {
        }
        int count;
        int skip;
    };

    window_values initial;

    window(int count, int skip)
        : initial(count, skip)
    {
    }

    struct window_observable : public sources::source_base<T>
    {
        mutable rxcpp::subjects::subject<T> subj;

        window_observable(rxcpp::subjects::subject<T> s)
            : subj(std::move(s))
        {
        }

        template<class Subscriber>
        void on_subscribe(Subscriber o) const {
            static_assert(is_subscriber<Subscriber>::value, "subscribe must be passed a subscriber");
            subj.get_observable().subscribe(o);
        }
    };

    template<class Subscriber>
    struct window_observer : public window_values, public observer_base<observable<T>>
    {
        typedef window_observer<Subscriber> this_type;
        typedef observer_base<observable<T>> base_type;
        typedef typename base_type::value_type value_type;
        typedef typename std::decay<Subscriber>::type dest_type;
        dest_type dest;
        mutable int cursor;
        mutable std::deque<rxcpp::subjects::subject<T>> subj;

        window_observer(dest_type d, window_values v)
            : window_values(v)
            , dest(std::move(d))
            , cursor(0)
        {
            subj.push_back(rxcpp::subjects::subject<T>());
            dest.on_next(observable<T, window_observable>(window_observable(subj[0])));
        }
        void on_next(T v) const {
            for (auto s : subj) {
                s.get_subscriber().on_next(v);
            }

            int c = cursor - this->count + 1;
            if (c >= 0 && c % this->skip == 0) {
                subj[0].get_subscriber().on_completed();
                subj.pop_front();
            }

            if (++cursor % this->skip == 0) {
                subj.push_back(rxcpp::subjects::subject<T>());
                dest.on_next(observable<T, window_observable>(window_observable(subj[subj.size() - 1])));
            }
        }

        void on_error(std::exception_ptr e) const {
            for (auto s : subj) {
                s.get_subscriber().on_error(e);
            }
            dest.on_error(e);
        }

        void on_completed() const {
            for (auto s : subj) {
                s.get_subscriber().on_completed();
            }
            dest.on_completed();
        }

        static subscriber<value_type, this_type> make(dest_type d, window_values v) {
            auto cs = d.get_subscription();
            return make_subscriber<value_type>(std::move(cs), this_type(std::move(d), std::move(v)));
        }
    };

    template<class Subscriber>
    auto operator()(Subscriber dest) const
        -> decltype(window_observer<Subscriber>::make(std::move(dest), initial)) {
        return      window_observer<Subscriber>::make(std::move(dest), initial);
    }
};

class window_factory
{
    int count;
    int skip;
public:
    window_factory(int c, int s) : count(c), skip(s) {}
    template<class Observable>
    auto operator()(Observable&& source)
        -> decltype(source.lift(window<typename std::decay<Observable>::type::value_type>(count, skip))) {
        return      source.lift(window<typename std::decay<Observable>::type::value_type>(count, skip));
    }
};

}

inline auto window(int count)
    ->      detail::window_factory {
    return  detail::window_factory(count, count);
}
inline auto window(int count, int skip)
    ->      detail::window_factory {
    return  detail::window_factory(count, skip);
}

}

}

#endif
