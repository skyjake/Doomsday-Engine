/*
 * Copyright (c) 2002-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __AMETHYST_SHARD_H__
#define __AMETHYST_SHARD_H__

class Source;

/**
 * The base class for all shards.
 * Provides tree linkage and access.
 * Shards represent the source file's structure.
 */
class Shard
{
public:
    enum ShardType { SHARD, TOKEN, BLOCK, COMMAND, GEM };

public:
    Shard(ShardType t = SHARD, Source *src = 0);
    virtual ~Shard();

    void clear();
    Shard *add(Shard *s);
    Shard *remove(Shard *s);    // s must be a child.
    ShardType type() { return _type; }
    Shard *parent() { return _parent; }
    Shard *next() { return _next; }
    Shard *prev() { return _prev; }
    Shard *first() { return _first; }
    Shard *last() { return _last; }
    Shard *child(int oneBasedIndex); // Negative index => reverse search.
    Shard *top();
    Shard *following();
    Shard *preceding();
    Shard *final();
    int count();
    int order();
    void setLineNumber(int num) { _lineNumber = num; }
    int lineNumber() { return _lineNumber; }
    void steal(Shard *whoseChildren);

    virtual bool isIdentical(Shard *other);
    bool operator == (Shard &other);

protected:
    ShardType   _type;

private:
    Shard       *_parent, *_next, *_prev, *_first, *_last;
    int         _lineNumber;
    Source      *_source;
};

#endif
