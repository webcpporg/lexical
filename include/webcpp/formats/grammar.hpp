// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The two types every rule of formats shares: the text a rule matched, and what a rule returns.

 @see "Errors", in the guide.
*/
#ifndef WEBCPP_FORMATS_GRAMMAR_HPP
#define WEBCPP_FORMATS_GRAMMAR_HPP

#include <webcpp/formats/config.hpp>

#include <boost/core/detail/string_view.hpp>
#include <boost/system/result.hpp>

namespace webcpp::formats::grammar {

/**
 The text a rule of formats matched, a view of its input.

 It is Boost.URL's text type, which converts to `std::string_view`.

 @see "Errors", in the guide.
*/
using text = boost::core::string_view;

/**
 What a rule returns: the value it matched, or the error that refused its input.

 @tparam T The rule's value type.
 @see "Errors", in the guide.
*/
template <class T>
using result = boost::system::result<T>;

}  // namespace webcpp::formats::grammar

#endif  // WEBCPP_FORMATS_GRAMMAR_HPP
