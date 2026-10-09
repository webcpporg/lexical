// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 Every format of formats: include this one header for all of them. It leaves out
 `<webcpp/formats/regex.hpp>`, which brings SRELL, a regular expression engine a program that
 never validates a regex should not compile; that header is included by name.

 @see "Introduction", in the guide.
*/
#ifndef WEBCPP_FORMATS_HPP
#define WEBCPP_FORMATS_HPP

#include <webcpp/formats/config.hpp>

#include <webcpp/formats/date_time.hpp>
#include <webcpp/formats/grammar.hpp>
#include <webcpp/formats/ip.hpp>
#include <webcpp/formats/json_pointer.hpp>
#include <webcpp/formats/uri.hpp>
#include <webcpp/formats/uuid.hpp>

#endif  // WEBCPP_FORMATS_HPP
