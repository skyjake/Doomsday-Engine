/** @file webrequest.h  Asynchronous GET/POST request.
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBCORE_WEBREQUEST_H
#define LIBCORE_WEBREQUEST_H

#include "de/block.h"
#include "de/string.h"
#include "de/observers.h"

namespace de {

/**
 * Asynchronous GET/POST request. Uses libcurl to receive and send data over the network.
 * @ingroup net
 */
class DE_PUBLIC WebRequest
{
public:
    DE_AUDIENCE(Progress,  void webRequestProgress(WebRequest &, dsize currentSize, dsize totalSize))
    DE_AUDIENCE(ReadyRead, void webRequestReadyRead(WebRequest &))
    DE_AUDIENCE(Finished,  void webRequestFinished(WebRequest &))

    /// The WebRequest object is busy pending the results of a started request. @ingroup errors
    DE_ERROR(PendingError);

public:
    WebRequest();

    void setUserAgent(const String &userAgent);

    void get(const String &url);
    void post(const String &url,
              const Block & content,
              const char *  httpContentType = "application/octet-stream");

    bool   isPending() const;
    bool   isFinished() const;
    bool   isSucceeded() const;
    bool   isFailed() const;
    String errorMessage() const;

    dsize contentLength() const;
    Block result() const;

    /**
     * Reads all data received so far and removes it from the result buffer.
     * This is useful if the data should be processed as it is received (e.g., streaming).
     * Note that called result() will not return anything after called readAll().
     *
     * @return Received bytes.
     */
    Block readAll();

public:
    static bool splitUriComponents(const String &uri,
                                   String *      scheme    = nullptr,
                                   String *      authority = nullptr,
                                   String *      path      = nullptr,
                                   String *      query     = nullptr,
                                   String *      fragment  = nullptr);

    static String hostNameFromUri(const String &uri);
    static String pathFromUri(const String &uri);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_WEBREQUEST_H
