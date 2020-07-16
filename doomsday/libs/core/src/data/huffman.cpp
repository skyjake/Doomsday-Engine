/**
 * @file huffman.cpp
 * Huffman codes.
 *
 * Uses predetermined, fixed frequencies optimized for short (size < 128)
 * messages.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "de/huffman.h"
#include "de/app.h"
#include "de/log.h"
#include "de/byterefarray.h"

// Heap relations.
#define HEAP_PARENT(i)  (((i) + 1)/2 - 1)
#define HEAP_LEFT(i)    (2*(i) + 1)
#define HEAP_RIGHT(i)   (2*(i) + 2)

namespace de {

namespace internal {

/*
 * Total number of bytes: 234457 (10217 packets)
 * Frequencies calculated in Doom II, co-op (1p)
 */
static double freqs[256] = {
    0.3108032603, 0.0030495997, 0.0035443599, 0.0023202549,
    0.0018638812, 0.0026188171, 0.0021752390, 0.0027083858,
    0.0175810490, 0.0011302712, 0.0010748240, 0.0015013414,
    0.0012241051, 0.0015951752, 0.0012923479, 0.0012795523,
    0.0011004150, 0.0013477951, 0.0434066801, 0.0016506225,
    0.0019790409, 0.0017146001, 0.0010108463, 0.0012113095,
    0.0014629548, 0.0013605906, 0.0015482583, 0.0017103349,
    0.0024055584, 0.0010151115, 0.0009980508, 0.0011558623,
    0.0015354628, 0.0012496961, 0.0015141369, 0.0021283220,
    0.0012241051, 0.0015311976, 0.0010534981, 0.0018510857,
    0.0013989772, 0.0013563255, 0.0015226673, 0.0012283702,
    0.0011302712, 0.0010790891, 0.0011601274, 0.0010236419,
    0.0013008782, 0.0012283702, 0.0013648558, 0.0011132105,
    0.0012624916, 0.0016165011, 0.0018596160, 0.0030240087,
    0.0018084340, 0.0013989772, 0.0013179389, 0.0012369006,
    0.0025932260, 0.0016719484, 0.0016463573, 0.0019406544,
    0.0122026640, 0.0017401912, 0.0144632065, 0.0403186938,
    0.0779332671, 0.0014970762, 0.0025207181, 0.0021027310,
    0.0018681464, 0.0014629548, 0.0014586897, 0.0011985140,
    0.0013563255, 0.0013094085, 0.0014928110, 0.0014586897,
    0.0015098717, 0.0014586897, 0.0012070444, 0.0017401912,
    0.0012454309, 0.0018126991, 0.0022264210, 0.0018297598,
    0.0027297116, 0.0012496961, 0.0013222041, 0.0016165011,
    0.0021453827, 0.0024695360, 0.0015994404, 0.0016676832,
    0.0011814533, 0.0021539131, 0.0013904469, 0.0015269324,
    0.0023586415, 0.0016420922, 0.0011558623, 0.0013819165,
    0.0012241051, 0.0013904469, 0.0013136737, 0.0020771399,
    0.0024865967, 0.0015482583, 0.0011899837, 0.0013136737,
    0.0012624916, 0.0016250315, 0.0017828429, 0.0014970762,
    0.0014629548, 0.0017529867, 0.0012411658, 0.0021411176,
    0.0023671718, 0.0019961016, 0.0015951752, 0.0025974912,
    0.0013051434, 0.0020728748, 0.0016079708, 0.0021283220,
    0.0550079546, 0.0033694878, 0.0025889609, 0.0021624434,
    0.0029728266, 0.0022946638, 0.0021283220, 0.0018510857,
    0.0020216927, 0.0017700474, 0.0018809419, 0.0015525235,
    0.0022562773, 0.0028832579, 0.0020899355, 0.0018425554,
    0.0024610056, 0.0020899355, 0.0017188653, 0.0021112613,
    0.0018638812, 0.0017231305, 0.0018254947, 0.0015951752,
    0.0020814051, 0.0020174275, 0.0019193285, 0.0014032424,
    0.0017572519, 0.0017913733, 0.0020003668, 0.0018510857,
    0.0022264210, 0.0012923479, 0.0017529867, 0.0018468205,
    0.0017359260, 0.0018596160, 0.0018084340, 0.0025463091,
    0.0011430667, 0.0022221559, 0.0010407026, 0.0012411658,
    0.0015354628, 0.0019235937, 0.0022178907, 0.0013819165,
    0.0021837693, 0.0015823797, 0.0013008782, 0.0011814533,
    0.0010492329, 0.0015695842, 0.0014160379, 0.0015823797,
    0.0014928110, 0.0019107981, 0.0012369006, 0.0019619802,
    0.0017913733, 0.0023799673, 0.0016037056, 0.0020174275,
    0.0148854587, 0.0032841843, 0.0018126991, 0.0023159897,
    0.0015056066, 0.0026955902, 0.0019747758, 0.0012624916,
    0.0011558623, 0.0014672200, 0.0017572519, 0.0022520121,
    0.0013136737, 0.0012752872, 0.0012411658, 0.0017743126,
    0.0014458941, 0.0012241051, 0.0012752872, 0.0017615170,
    0.0012113095, 0.0011515971, 0.0013776513, 0.0010748240,
    0.0016250315, 0.0012283702, 0.0014117727, 0.0009596642,
    0.0011430667, 0.0010705588, 0.0013264692, 0.0012923479,
    0.0025889609, 0.0013733862, 0.0013136737, 0.0012752872,
    0.0014970762, 0.0011899837, 0.0013691210, 0.0010023160,
    0.0014416290, 0.0010876195, 0.0010662936, 0.0009340732,
    0.0011814533, 0.0010577633, 0.0012710220, 0.0017316608,
    0.0014586897, 0.0010449677, 0.0017359260, 0.0010279070,
    0.0016292966, 0.0018297598, 0.0020259579, 0.0015311976,
    0.0040775067, 0.0010790891, 0.0013861817, 0.0010108463,
    0.0017103349, 0.0012496961, 0.0022903987, 0.0028619320
};

struct HuffNode {
    HuffNode *left, *right;
    double freq;
    dbyte value;              // Only valid for leaves.
};

struct HuffQueue {
    HuffNode *nodes[256];
    int count;
};

struct HuffCode {
    duint code;
    duint length;
};

struct HuffBuffer {
    dbyte *data;
    dsize size;
};

struct Huffman
{
    // The root of the Huffman tree.
    HuffNode *huffRoot;

    // The lookup table for encoding.
    HuffCode huffCodes[256];

    /**
     * Builds the Huffman tree and initializes the code lookup.
     */
    Huffman() : huffRoot(0)
    {
        zap(huffCodes);

        HuffQueue queue;
        HuffNode *node;
        int i;

        // Initialize the priority queue that holds the remaining nodes.
        queue.count = 0;
        for (i = 0; i < 256; ++i)
        {
            // These are the leaves of the tree.
            node = (HuffNode *) calloc(1, sizeof(HuffNode));
            node->freq = freqs[i];
            node->value = i;
            Huff_QueueInsert(&queue, node);
        }

        // Build the tree.
        for (i = 0; i < 255; ++i)
        {
            node = (HuffNode *) calloc(1, sizeof(HuffNode));
            node->left = Huff_QueueExtract(&queue);
            node->right = Huff_QueueExtract(&queue);
            node->freq = node->left->freq + node->right->freq;
            Huff_QueueInsert(&queue, node);
        }

        // The root is the last node left in the queue.
        huffRoot = Huff_QueueExtract(&queue);

        // Fill in the code lookup table.
        Huff_BuildLookup(huffRoot, 0, 0);

#if 0
        if (qApp->arguments().contains("-huffcodes"))
        {
            double realBits = 0;
            double huffBits = 0;

            LOG_MSG("Huffman codes:");
            for (i = 0; i < 256; ++i)
            {
                uint k;
                realBits += freqs[i] * 8;
                huffBits += freqs[i] * huffCodes[i].length;
                LOG_MSG("%03i: (%07i) ", i, (int) (6e6 * freqs[i]));
                for (k = 0; k < huffCodes[i].length; ++k)
                {
                    LogBuffer_Printf(0, "%c", huffCodes[i].code & (1 << k) ? '1' : '0');
                }
                LogBuffer_Printf(0, "\n");
            }
            LOG_MSG("realbits=%f, huffbits=%f (%f%%)")
                    << realBits <<huffBits << huffBits / realBits * 100;
        }
#endif
    }

    /**
     * Free all resources allocated for Huffman codes.
     */
    ~Huffman()
    {
        Huff_DestroyNode(huffRoot);
        huffRoot = NULL;
    }

    /**
     * Exchange two nodes in the queue.
     */
    void Huff_QueueExchange(HuffQueue *queue, int index1, int index2)
    {
        HuffNode *temp = queue->nodes[index1];
        queue->nodes[index1] = queue->nodes[index2];
        queue->nodes[index2] = temp;
    }

    /**
     * Insert a node into a priority queue.
     */
    void Huff_QueueInsert(HuffQueue *queue, HuffNode *node)
    {
        int i, parent;

        // Add the new node to the end of the queue.
        i = queue->count;
        queue->nodes[i] = node;
        ++queue->count;

        // Rise in the heap until the correct place is found.
        while (i > 0)
        {
            parent = HEAP_PARENT(i);

            // Is it good now?
            if (queue->nodes[parent]->freq <= node->freq)
                break;

            // Exchange with the parent.
            Huff_QueueExchange(queue, parent, i);

            i = parent;
        }
    }

    /**
     * Extract the smallest node from the queue.
     */
    HuffNode *Huff_QueueExtract(HuffQueue *queue)
    {
        HuffNode *min;
        int i, left, right, small;

        DE_ASSERT(queue->count > 0);

        // This is what we'll return.
        min = queue->nodes[0];

        // Remove the first element from the queue.
        queue->nodes[0] = queue->nodes[--queue->count];

        // Heapify the heap. This is O(log n).
        i = 0;
        for (;;)
        {
            left = HEAP_LEFT(i);
            right = HEAP_RIGHT(i);
            small = i;

            // Which child has smaller freq?
            if (left < queue->count &&
               queue->nodes[left]->freq < queue->nodes[i]->freq)
            {
                small = left;
            }
            if (right < queue->count &&
               queue->nodes[right]->freq < queue->nodes[small]->freq)
            {
                small = right;
            }

            // Can we stop now?
            if (i == small)
            {
                // Heapifying is complete.
                break;
            }

            // Exchange and continue.
            Huff_QueueExchange(queue, i, small);
            i = small;
        }

        return min;
    }

    /**
     * Recursively builds the Huffman code lookup for the node's subtree.
     */
    void Huff_BuildLookup(HuffNode *node, uint code, uint length)
    {
        if (!node->left && !node->right)
        {
            // This is a leaf.
            huffCodes[node->value].code = code;
            huffCodes[node->value].length = length;
            return;
        }

        // Shouldn't run out of bits...
        DE_ASSERT(length < 32);

        // Descend into the left and right subtrees.
        if (node->left)
        {
            // This child's bit is zero.
            Huff_BuildLookup(node->left, code, length + 1);
        }
        if (node->right)
        {
            // This child's bit is one.
            Huff_BuildLookup(node->right, code | (1 << length), length + 1);
        }
    }

    /**
     * Checks if the encoding/decoding buffer can hold the given number of
     * bytes. If not, reallocates the buffer.
     */
    static void Huff_ResizeBuffer(HuffBuffer *buffer, dsize neededSize)
    {
        while (neededSize > buffer->size)
        {
            if (!buffer->size)
                buffer->size = std::max(dsize(1024), neededSize);
            else
                buffer->size *= 2;
        }
        buffer->data = reinterpret_cast<dbyte *>(realloc(buffer->data, buffer->size));
    }

    /**
     * Recursively frees the node and its subtree.
     */
    void Huff_DestroyNode(HuffNode *node)
    {
        if (node)
        {
            Huff_DestroyNode(node->left);
            Huff_DestroyNode(node->right);
            free(node);
        }
    }

    /**
     * Free the buffer.
     */
    void Huff_DestroyBuffer(HuffBuffer *buffer)
    {
        free(buffer->data);
        zapPtr(buffer);
    }

    dbyte *encode(const dbyte *data, dsize size, dsize *encodedSize) const
    {
        HuffBuffer huffEnc;
        dsize i;
        duint code;
        int remaining, fits;
        dbyte *out, bit;

        zap(huffEnc);

        // The encoded message is never twice the original size
        // (longest codes are currently 11 bits).
        Huff_ResizeBuffer(&huffEnc, 2 * size);

        // First three bits of the encoded data contain the number of bits (-1)
        // in the last dbyte of the encoded data. It's written when we have
        // finished the encoding.
        bit = 3;

        // Clear the first dbyte of the result.
        out = huffEnc.data;
        *out = 0;

        for (i = 0; i < size; ++i)
        {
            remaining = huffCodes[data[i]].length;
            code = huffCodes[data[i]].code;

            while (remaining > 0)
            {
                fits = 8 - bit;
                if (fits > remaining)
                    fits = remaining;

                // Write the bits that fit the current dbyte.
                *out |= code << bit;
                code >>= fits;
                remaining -= fits;

                // Advance the bit position.
                bit += fits;
                if (bit == 8)
                {
                    bit = 0;
                    *++out = 0;
                }
            }
        }

        // If the last dbyte is empty, back up.
        if (bit == 0)
        {
            out--;
            bit = 8;
        }

        if (encodedSize)
            *encodedSize = out - huffEnc.data + 1;

        // The number of valid bits - 1 in the last dbyte.
        huffEnc.data[0] |= bit - 1;

        return huffEnc.data;
    }

    dbyte *decode(const dbyte *data, dsize size, dsize *decodedSize) const
    {
        HuffBuffer huffDec;
        HuffNode *node;
        dsize outBytes = 0;
        const dbyte *in = data;
        const dbyte *lastIn = in + size - 1;
        dbyte bit = 3, lastByteBits;

        if (!data || size == 0) return nullptr;

        zap(huffDec);
        Huff_ResizeBuffer(&huffDec, 256);

        // The first three bits contain the number of valid bits in
        // the last dbyte.
        lastByteBits = (*in & 7) + 1;

        // Start from the root node.
        node = huffRoot;

        while (in < lastIn || bit < lastByteBits)
        {
            // Go left or right?
            if (*in & (1 << bit))
            {
                node = node->right;
            }
            else
            {
                node = node->left;
            }

            // Did we arrive at a leaf?
            DE_ASSERT(node);
            if (!node->left && !node->right)
            {
                // This node represents a value.
                huffDec.data[outBytes++] = node->value;

                // Should we allocate more memory?
                if (outBytes == huffDec.size)
                {
                    Huff_ResizeBuffer(&huffDec, 2 * huffDec.size);
                }

                // Back to the root.
                node = huffRoot;
            }

            // Advance bit position.
            if (++bit == 8)
            {
                bit = 0;
                ++in;

                // Out of buffer?
                if (in > lastIn)
                    break;
            }
        }
        if (decodedSize)
        {
            *decodedSize = outBytes;
        }
        return huffDec.data;
    }
};

} // namespace internal

static internal::Huffman huff;

Block codec::huffmanEncode(const Block &data)
{
    Block result;
    dsize size = 0;
    dbyte *coded = huff.encode(data.data(), data.size(), &size);
    if (coded)
    {
        result.copyFrom(ByteRefArray(coded, size), 0, size);
        free(coded);
    }
    return result;
}

Block codec::huffmanDecode(const Block &codedData)
{
    Block result;
    dsize size = 0;
    dbyte *decoded = huff.decode(codedData.data(), codedData.size(), &size);
    if (decoded)
    {
        result.copyFrom(ByteRefArray(decoded, size), 0, size);
        free(decoded);
    }
    return result;
}

} // namespace de
