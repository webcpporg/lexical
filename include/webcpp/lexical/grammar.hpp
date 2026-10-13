// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The two types every rule of lexical shares: the text a rule matched, and what a rule returns.

 @see "Composing rules", in the guide.
*/
#ifndef WEBCPP_LEXICAL_GRAMMAR_HPP
#define WEBCPP_LEXICAL_GRAMMAR_HPP

#include <webcpp/lexical/config.hpp>

#include <boost/core/detail/string_view.hpp>
#include <boost/system/result.hpp>

namespace webcpp::lexical::grammar {

/**
 The text a rule of lexical matched, a view of its input.

 It is Boost.URL's text type, which converts to `std::string_view`.

 @see "Composing rules", in the guide.
*/
using text = boost::core::string_view;

/**
 What a rule returns: the value it matched, or the error that refused its input.

 @tparam T The rule's value type.
 @see "Errors", in the guide.
*/
template <class T>
using result = boost::system::result<T>;

}  // namespace webcpp::lexical::grammar

#endif  // WEBCPP_LEXICAL_GRAMMAR_HPP
