// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 Every format of lexical: include this one header for all of them. It leaves out
 `<webcpp/lexical/regex.hpp>`, which brings the Unicode tables of ECMA-262's classes; a program
 that never validates a regex should not compile them, so that header is included by name.

 @see "Introduction", in the guide.
*/
#ifndef WEBCPP_LEXICAL_HPP
#define WEBCPP_LEXICAL_HPP

#include <webcpp/lexical/config.hpp>

#include <webcpp/lexical/date_time.hpp>
#include <webcpp/lexical/email.hpp>
#include <webcpp/lexical/grammar.hpp>
#include <webcpp/lexical/hostname.hpp>
#include <webcpp/lexical/ip.hpp>
#include <webcpp/lexical/json_pointer.hpp>
#include <webcpp/lexical/uri.hpp>
#include <webcpp/lexical/uri_template.hpp>
#include <webcpp/lexical/uuid.hpp>

#endif  // WEBCPP_LEXICAL_HPP
