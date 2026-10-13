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

#include <webcpp/config.hpp>

// MrDocs parses formats as a build with exceptions does, where the macro is not defined: this
// definition, undone at once, is the one the reference lists, and the parse goes on with every API
// that throws in it.
#ifdef __MRDOCS__
/**
 Defined when formats is built without exceptions: when webcpp has none (the compiler has none,
 or the developer defined `WEBCPP_NO_EXCEPTIONS` for every webcpp library), or when the developer
 defines it alone, to disable formats' in a build that has them.

 formats raises no exception of its own: a validator answers `false` and a rule returns its error,
 so the macro changes nothing in it. It is provided so that a program can set it for every webcpp
 library alike. Only the memory an IRI function allocates may throw, and an exception formats
 raised would go through `boost::throw_exception`.

 @see "Errors", in the guide.
*/
#define WEBCPP_FORMATS_NO_EXCEPTIONS
#undef WEBCPP_FORMATS_NO_EXCEPTIONS
#endif

#if !defined(WEBCPP_HAS_EXCEPTIONS) && !defined(WEBCPP_FORMATS_NO_EXCEPTIONS)
#define WEBCPP_FORMATS_NO_EXCEPTIONS
#endif

#endif  // WEBCPP_FORMATS_CONFIG_HPP
