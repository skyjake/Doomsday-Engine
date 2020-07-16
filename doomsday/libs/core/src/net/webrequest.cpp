/** @file webrequest.cpp  Asynchronous GET/POST request.
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

#include "de/webrequest.h"
#include "de/regexp.h"
#include "de/async.h"

#include <the_Foundation/webrequest.h>

namespace de {

DE_PIMPL(WebRequest), public Lockable, public AsyncScope
{
    enum Status {
        Initialized,
        Pending,
        Success,
        Failure,
    };
    enum Method { Get, Post };

    String userAgent;
    Status status = Initialized;
    tF::ref<iWebRequest> web;

    Impl(Public *i) : Base(i)
    {
        web = tF::make_ref(new_WebRequest());
        setUserData_Object(web, this);
        iConnect(WebRequest, web, progress,  web, notifyProgress);
        iConnect(WebRequest, web, readyRead, web, notifyReadyRead);
    }

    static void notifyProgress(iAny *, iWebRequest *web, size_t current, size_t total)
    {
        Loop::mainCall([web, current, total]() {
            auto *d = reinterpret_cast<Impl *>(userData_Object(web));
            DE_FOR_OBSERVERS(i, d->audienceForProgress)
            {
                i->webRequestProgress(d->self(), current, total);
            }
        });
    }

    static void notifyReadyRead(iAny *, iWebRequest *web)
    {
        Loop::mainCall([web]() {
            auto *d = reinterpret_cast<Impl *>(userData_Object(web));
            DE_FOR_OBSERVERS(i, d->audienceForReadyRead)
            {
                i->webRequestReadyRead(d->self());
            }
        });
    }

    void startAsync(Method method)
    {
        DE_GUARD(this);

        if (status == Pending)
        {
            throw PendingError("WebRequest::start",
                               "Cannot start a new request, previous one is still pending");
        }
        status = Pending;
        if (userAgent)
        {
            setUserAgent_WebRequest(web, userAgent);
        }
        *this += async([this, method]() {
            return method == Get ? get_WebRequest(web) : post_WebRequest(web);
        },
        [this](iBool ok) {
            {
                DE_GUARD(this);
                status = (ok ? Success : Failure);
            }
            DE_NOTIFY_PUBLIC(Finished, i) { i->webRequestFinished(self()); }
        });
    }

    DE_PIMPL_AUDIENCES(Progress, ReadyRead, Finished)
};

DE_AUDIENCE_METHODS(WebRequest, Progress, ReadyRead, Finished)

WebRequest::WebRequest()
    : d(new Impl(this))
{}

void WebRequest::setUserAgent(const String &userAgent)
{
    DE_GUARD(d);
    d->userAgent = userAgent;
}

void WebRequest::get(const String &url)
{
    DE_GUARD(d);
    clear_WebRequest(d->web);
    setUrl_WebRequest(d->web, url);
    d->startAsync(Impl::Get);
}

void WebRequest::post(const String &url, const Block &content, const char *httpContentType)
{
    DE_GUARD(d);
    clear_WebRequest(d->web);
    setUrl_WebRequest(d->web, url);
    setPostData_WebRequest(d->web, httpContentType, content);
    d->startAsync(Impl::Post);
}

bool WebRequest::isPending() const
{
    DE_GUARD(d);
    return d->status == Impl::Pending;
}

bool WebRequest::isFinished() const
{
    DE_GUARD(d);
    return d->status == Impl::Success || d->status == Impl::Failure;
}

bool WebRequest::isSucceeded() const
{
    DE_GUARD(d);
    return d->status == Impl::Success;
}

bool WebRequest::isFailed() const
{
    DE_GUARD(d);
    return d->status == Impl::Failure;
}

String WebRequest::errorMessage() const
{
    DE_GUARD(d);
    return errorMessage_WebRequest(d->web);
}

dsize WebRequest::contentLength() const
{
    DE_GUARD(d);
    return contentLength_WebRequest(d->web);
}

Block WebRequest::result() const
{
    DE_GUARD(d);
    return result_WebRequest(d->web);
}

Block WebRequest::readAll()
{
    DE_GUARD(d);
    return Block::take(read_WebRequest(d->web));
}

bool WebRequest::splitUriComponents(const String &uri,
                                    String *      scheme,
                                    String *      authority,
                                    String *      path,
                                    String *      query,
                                    String *      fragment)
{
    static const RegExp reComps(R"(^(([A-Za-z0-9.-]+):)?(//([^/\?#]*))?([^\?#]*)(\?([^#]*))?(#(.*))?)");
    RegExpMatch m;
    if (reComps.match(uri, m))
    {
        if (scheme)    *scheme    = m.captured(2);
        if (authority) *authority = m.captured(4);
        if (path)      *path      = m.captured(5);
        if (query)     *query     = m.captured(7);
        if (fragment)  *fragment  = m.captured(9);
        return true;
    }
    return false;
}

String WebRequest::hostNameFromUri(const String &uri)
{
    String authority;
    if (splitUriComponents(uri, nullptr, &authority))
    {
        static const RegExp reAuth(R"(([^@:]+@)?(\[[0-9A-Za-z:%]+\]|[^:]+)(:([0-9]+))?)");
        RegExpMatch m;
        if (reAuth.match(authority, m))
        {
            return m.captured(2);
        }
    }
    return {};
}

String WebRequest::pathFromUri(const String &uri)
{
    String path;
    splitUriComponents(uri, nullptr, nullptr, &path);
    return path;
}

} // namespace de
