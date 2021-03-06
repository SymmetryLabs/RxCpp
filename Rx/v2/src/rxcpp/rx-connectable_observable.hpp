// Copyright (c) Microsoft Open Technologies, Inc. All rights reserved. See License.txt in the project root for license information.

#pragma once

#if !defined(RXCPP_RX_CONNECTABLE_OBSERVABLE_HPP)
#define RXCPP_RX_CONNECTABLE_OBSERVABLE_HPP

#include "rx-includes.hpp"

namespace rxcpp {

namespace detail {

template<class T>
struct has_on_connect
{
    struct not_void {};
    template<class CT>
    static auto check(int) -> decltype((*(CT*)nullptr).on_connect(composite_subscription()));
    template<class CT>
    static not_void check(...);

    typedef decltype(check<T>(0)) detail_result;
    static const bool value = std::is_same<detail_result, void>::value;
};

}

template<class T>
class dynamic_connectable_observable
    : public dynamic_observable<T>
{
    struct state_type
        : public std::enable_shared_from_this<state_type>
    {
        typedef std::function<void(composite_subscription)> onconnect_type;

        onconnect_type on_connect;
    };
    std::shared_ptr<state_type> state;

    template<class U>
    void construct(const dynamic_observable<U>& o, tag_dynamic_observable&&) {
        state = o.state;
    }

    template<class U>
    void construct(dynamic_observable<U>&& o, tag_dynamic_observable&&) {
        state = std::move(o.state);
    }

    template<class SO>
    void construct(SO&& source, rxs::tag_source&&) {
        auto so = std::make_shared<rxu::decay_t<SO>>(std::forward<SO>(source));
        state->on_connect = [so](composite_subscription cs) mutable {
            so->on_connect(std::move(cs));
        };
    }

public:

    typedef tag_dynamic_observable dynamic_observable_tag;

    dynamic_connectable_observable()
    {
    }

    template<class SOF>
    explicit dynamic_connectable_observable(SOF sof)
        : dynamic_observable<T>(sof)
        , state(std::make_shared<state_type>())
    {
        construct(std::move(sof),
                  typename std::conditional<is_dynamic_observable<SOF>::value, tag_dynamic_observable, rxs::tag_source>::type());
    }

    template<class SF, class CF>
    dynamic_connectable_observable(SF&& sf, CF&& cf)
        : dynamic_observable<T>(std::forward<SF>(sf))
        , state(std::make_shared<state_type>())
    {
        state->on_connect = std::forward<CF>(cf);
    }

    using dynamic_observable<T>::on_subscribe;

    void on_connect(composite_subscription cs) const {
        state->on_connect(std::move(cs));
    }
};

template<class T, class Source>
connectable_observable<T> make_dynamic_connectable_observable(Source&& s) {
    return connectable_observable<T>(dynamic_connectable_observable<T>(std::forward<Source>(s)));
}


/*!
    \brief a source of values that is shared across all subscribers and does not start until connectable_observable::connect() is called.

    \ingroup group-observable

*/
template<class T, class SourceOperator>
class connectable_observable
    : public observable<T, SourceOperator>
{
    typedef connectable_observable<T, SourceOperator> this_type;
    typedef observable<T, SourceOperator> base_type;
    typedef rxu::decay_t<SourceOperator> source_operator_type;

    static_assert(detail::has_on_connect<source_operator_type>::value, "inner must have on_connect method void(composite_subscription)");

public:
    typedef tag_connectable_observable observable_tag;

    connectable_observable()
    {
    }

    explicit connectable_observable(const SourceOperator& o)
        : base_type(o)
    {
    }
    explicit connectable_observable(SourceOperator&& o)
        : base_type(std::move(o))
    {
    }

    // implicit conversion between observables of the same value_type
    template<class SO>
    connectable_observable(const connectable_observable<T, SO>& o)
        : base_type(o)
    {}
    // implicit conversion between observables of the same value_type
    template<class SO>
    connectable_observable(connectable_observable<T, SO>&& o)
        : base_type(std::move(o))
    {}

    ///
    /// performs type-forgetting conversion to a new composite_observable
    ///
    connectable_observable<T> as_dynamic() {
        return *this;
    }

    composite_subscription connect(composite_subscription cs = composite_subscription()) {
        base_type::source_operator.on_connect(cs);
        return cs;
    }

    /// ref_count ->
    /// takes a connectable_observable source and uses a ref_count of the subscribers
    /// to control the connection to the published source. The first subscription
    /// will cause a call to connect() and the last unsubscribe will unsubscribe the
    /// connection.
    ///
    auto ref_count() const
        ->      observable<T,   rxo::detail::ref_count<T, this_type>> {
        return  observable<T,   rxo::detail::ref_count<T, this_type>>(
                                rxo::detail::ref_count<T, this_type>(*this));
    }

    /// connect_forever ->
    /// takes a connectable_observable source and calls connect during
    /// the construction of the expression. This means that the source
    /// starts running without any subscribers and continues running
    /// after all subscriptions have been unsubscribed.
    ///
    auto connect_forever() const
        ->      observable<T,   rxo::detail::connect_forever<T, this_type>> {
        return  observable<T,   rxo::detail::connect_forever<T, this_type>>(
                                rxo::detail::connect_forever<T, this_type>(*this));
    }
};


}

//
// support range() >> filter() >> subscribe() syntax
// '>>' is spelled 'stream'
//
template<class T, class SourceOperator, class OperatorFactory>
auto operator >> (const rxcpp::connectable_observable<T, SourceOperator>& source, OperatorFactory&& of)
    -> decltype(source.op(std::forward<OperatorFactory>(of))) {
    return      source.op(std::forward<OperatorFactory>(of));
}

//
// support range() | filter() | subscribe() syntax
// '|' is spelled 'pipe'
//
template<class T, class SourceOperator, class OperatorFactory>
auto operator | (const rxcpp::connectable_observable<T, SourceOperator>& source, OperatorFactory&& of)
    -> decltype(source.op(std::forward<OperatorFactory>(of))) {
    return      source.op(std::forward<OperatorFactory>(of));
}

#endif
