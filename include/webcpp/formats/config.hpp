// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The configuration of formats, which each of its headers includes first: whether formats is built
 without exceptions.

 @see "Errors", in the guide.
*/
#ifndef WEBCPP_FORMATS_CONFIG_HPP
#define WEBCPP_FORMATS_CONFIG_HPP

// Boost.Config says whether the build has exceptions, until webcpp::config exists. The migration
// is one line here, this include becoming <webcpp/config.hpp>, with
// <library>/webcpp/config//config added to build.jam's alias in the same commit.
#include <boost/config.hpp>

// MrDocs parses formats as a build with exceptions does, where the macro is not defined: this
// definition, undone at once, is the one the reference lists.
#ifdef __MRDOCS__
/**
 Defined when formats is built without exceptions: automatically when the compiler has none
 (`BOOST_NO_EXCEPTIONS`), or by the developer to disable them in a build that has them.

 formats raises no exception of its own: a validator answers `false` and a rule returns its error,
 so the macro changes nothing in it. It is provided so that a program can set it for every webcpp
 library alike. Only a memory resource given to an IRI function or to `is_regex` may throw, and
 an exception formats raised would go through `boost::throw_exception`.

 @see "Errors", in the guide.
*/
#define WEBCPP_FORMATS_NO_EXCEPTIONS
#undef WEBCPP_FORMATS_NO_EXCEPTIONS
#endif

#if defined(BOOST_NO_EXCEPTIONS) && !defined(WEBCPP_FORMATS_NO_EXCEPTIONS)
#define WEBCPP_FORMATS_NO_EXCEPTIONS
#endif

#endif  // WEBCPP_FORMATS_CONFIG_HPP
