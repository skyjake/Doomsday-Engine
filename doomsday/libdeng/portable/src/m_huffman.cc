/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * m_huffman.c: Huffman Codes
 *
 * Huffman encoding/decoding using predetermined, fixed frequencies.
 * NOT THREAD-SAFE (static buffers used for storing data)
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_misc.h"
#include "doomsday.h"

// MACROS ------------------------------------------------------------------

//#define PRINT_CODES

// TYPES -------------------------------------------------------------------

typedef struct huffnode_s {
    struct huffnode_s *left, *right;
    double  freq;
    byte    value;              // Only valid for leaves.
} huffnode_t;

typedef struct huffqueue_s {
    huffnode_t *nodes[256];
    int     count;
} huffqueue_t;

typedef struct huffcode_s {
    uint    code;
    uint    length;
} huffcode_t;

typedef struct huffbuffer_s {
    byte   *data;
    uint    size;
} huffbuffer_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

/*
 * Total number of bytes: 4857074
 * Frequencies calculated in jHexen, co-op (1p) hub 2, map4, map5
 * (12 min, frame interval 0).
 *
 * Optimal compression: 69.6%
 */
static double freqs[256] = {
    0.261347881463, 0.016916769232, 0.099777149782, 0.007231102512,
    0.078927148320, 0.003303840954, 0.003997674320, 0.051381346053,
    0.004863421887, 0.002004704890, 0.023628011432, 0.001021396833,
    0.010792094170, 0.001282047587, 0.001111368696, 0.000778452212,
    0.005905613133, 0.000729039747, 0.002841010864, 0.011992199419,
    0.005981378912, 0.000727804435, 0.005555402285, 0.000820864578,
    0.003674846214, 0.001290077112, 0.001652229305, 0.002900099937,
    0.001201340560, 0.000922983673, 0.001000602420, 0.000942748659,
    0.008744976914, 0.000595008435, 0.001374078303, 0.001144927996,
    0.003616374797, 0.001251164796, 0.001358019252, 0.001723053839,
    0.000872747667, 0.001004926011, 0.000651009229, 0.001242517615,
    0.001207723004, 0.001599728561, 0.001360901646, 0.001332695364,
    0.004142205781, 0.001597669708, 0.001850702707, 0.001053309050,
    0.000786069967, 0.001016867357, 0.002317650503, 0.000990308157,
    0.001347519103, 0.001928115569, 0.001500491860, 0.002002234267,
    0.001933674472, 0.001370784139, 0.001208134774, 0.001714612542,
    0.005039453795, 0.002550918516, 0.002118353560, 0.003177839168,
    0.001923380208, 0.001494932958, 0.001643582124, 0.001712553690,
    0.001529521683, 0.001255900157, 0.001523345125, 0.001288635915,
    0.001455402985, 0.000617861700, 0.000676333117, 0.000837541285,
    0.003083749599, 0.000586361254, 0.000994425862, 0.001056191444,
    0.000882836045, 0.002295209009, 0.001077397627, 0.001015014389,
    0.001331665937, 0.001822084654, 0.003632227963, 0.001015837930,
    0.000865335797, 0.000736863387, 0.000598302599, 0.000684774414,
    0.003247016619, 0.001498638892, 0.000759098997, 0.000711745384,
    0.000926895493, 0.000828482333, 0.000836100088, 0.000679215511,
    0.001347724988, 0.000768775604, 0.000724510271, 0.001178487295,
    0.000712363040, 0.000693009824, 0.000760951964, 0.000659656410,
    0.006524298374, 0.000662950575, 0.000968690203, 0.001575022328,
    0.001306547934, 0.001461785429, 0.001409078799, 0.000990514042,
    0.001219870235, 0.002090147278, 0.001233664548, 0.000780511065,
    0.000655950476, 0.000589037762, 0.005355281801, 0.002871275999,
    0.012205908331, 0.001121457075, 0.001259194322, 0.001241282303,
    0.000553213725, 0.000456447647, 0.000562272677, 0.000433594382,
    0.000508124850, 0.000560625595, 0.000521095623, 0.000535095821,
    0.000518007344, 0.000676539003, 0.000595626091, 0.000676744888,
    0.011022685675, 0.001302841999, 0.001583257739, 0.001272782749,
    0.001249105943, 0.001152751636, 0.000997308256, 0.000526448640,
    0.000746128225, 0.000989484616, 0.001275047487, 0.001116309943,
    0.001525403978, 0.001194134576, 0.001201134675, 0.000638244342,
    0.009756079483, 0.000558566742, 0.000810364429, 0.000763010817,
    0.001378607779, 0.001397137454, 0.001303253770, 0.001260841404,
    0.001317253968, 0.001550110210, 0.001201752331, 0.001166340064,
    0.001016867357, 0.001269282700, 0.001046514836, 0.000477653830,
    0.009010363029, 0.000524595672, 0.000525419213, 0.000557743201,
    0.000509154277, 0.000721216107, 0.000516977917, 0.000750040045,
    0.000867188764, 0.000793893608, 0.000699186383, 0.001072662265,
    0.000882630160, 0.000708039449, 0.000686627381, 0.000714010122,
    0.009556370770, 0.000669333018, 0.000901983375, 0.001089338972,
    0.001178899066, 0.001021808603, 0.001060309149, 0.001137927897,
    0.001738083463, 0.001676935538, 0.001687847457, 0.001666435389,
    0.001912468289, 0.001787084158, 0.001850702707, 0.003220869190,
    0.010349029066, 0.002433769796, 0.002500064854, 0.001287194718,
    0.001127839518, 0.001192487493, 0.001099839121, 0.001036632343,
    0.001130310141, 0.001100868548, 0.001021808603, 0.000994631747,
    0.001126810092, 0.001035602916, 0.001080485906, 0.001121868845,
    0.009375809386, 0.001078632938, 0.001236546942, 0.001240870532,
    0.001318901050, 0.001059897379, 0.001052691394, 0.001085221267,
    0.001181369689, 0.000856276845, 0.001283900554, 0.001125163010,
    0.001325901150, 0.001437902737, 0.001482373956, 0.001487726973,
    0.005829023811, 0.001277929881, 0.001573787017, 0.001741377628,
    0.001309636213, 0.001333930675, 0.000908983474, 0.001500080089,
    0.001828055327, 0.001625670105, 0.001895379811, 0.002661273022,
    0.003641698685, 0.003401018803, 0.005049130402, 0.010072525146
};

// The root of the Huffman tree.
static huffnode_t *huffRoot;

// The lookup table for encoding.
static huffcode_t huffCodes[256];

// The buffer used for the result of encoding/decoding.
static huffbuffer_t huffEnc, huffDec;

// CODE --------------------------------------------------------------------

/**
 * Exchange two nodes in the queue.
 */
static void Huff_QueueExchange(huffqueue_t *queue, int index1, int index2)
{
    huffnode_t *temp = queue->nodes[index1];

    queue->nodes[index1] = queue->nodes[index2];
    queue->nodes[index2] = temp;
}

/**
 * Insert a node into a priority queue.
 */
static void Huff_QueueInsert(huffqueue_t *queue, huffnode_t *node)
{
    int     i, parent;

    // Add the new node to the end of the queue.
    i = queue->count;
    queue->nodes[i] = node;
    ++queue->count;

    // Rise in the heap until the correct place is found.
    while(i > 0)
    {
        parent = HEAP_PARENT(i);

        // Is it good now?
        if(queue->nodes[parent]->freq <= node->freq)
            break;

        // Exchange with the parent.
        Huff_QueueExchange(queue, parent, i);

        i = parent;
    }
}

/**
 * Extract the smallest node from the queue.
 */
static huffnode_t *Huff_QueueExtract(huffqueue_t *queue)
{
    huffnode_t *min;
    int     i, left, right, small;

    if(!queue->count)
    {
        Con_Error("Huff_QueueExtract: Out of nodes.\n");
    }

    // This is what we'll return.
    min = queue->nodes[0];

    // Remove the first element from the queue.
    queue->nodes[0] = queue->nodes[--queue->count];

    // Heapify the heap. This is O(log n).
    i = 0;
    for(;;)
    {
        left = HEAP_LEFT(i);
        right = HEAP_RIGHT(i);
        small = i;

        // Which child has smaller freq?
        if(left < queue->count &&
           queue->nodes[left]->freq < queue->nodes[i]->freq)
        {
            small = left;
        }
        if(right < queue->count &&
           queue->nodes[right]->freq < queue->nodes[small]->freq)
        {
            small = right;
        }

        // Can we stop now?
        if(i == small)
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
static void Huff_BuildLookup(huffnode_t *node, uint code, uint length)
{
    if(!node->left && !node->right)
    {
        // This is a leaf.
        huffCodes[node->value].code = code;
        huffCodes[node->value].length = length;
        return;
    }

    if(length == 32)
    {
        Con_Error("Huff_BuildLookup: Out of bits, length = %i\n", length);
    }

    // Descend into the left and right subtrees.
    if(node->left)
    {
        // This child's bit is zero.
        Huff_BuildLookup(node->left, code, length + 1);
    }
    if(node->right)
    {
        // This child's bit is one.
        Huff_BuildLookup(node->right, code | (1 << length), length + 1);
    }
}

/**
 * Checks if the encoding/decoding buffer can hold the given number of
 * bytes. If not, reallocates the buffer.
 */
static void Huff_CheckBuffer(huffbuffer_t *buffer, size_t neededSize)
{
    byte           *tempbuffer;

    while(neededSize > buffer->size)
    {
        buffer->size *= 2;
        if(!buffer->size)
            buffer->size = 0x2000;

        if((tempbuffer = (byte*) M_Realloc(buffer->data, buffer->size)) == NULL)
            Con_Error("Huff_CheckBuffer: realloc failed.");

        buffer->data = tempbuffer;
    }
}

/**
 * Build the Huffman tree and initialize the code lookup.
 * (CLR 2nd Ed, p. 388)
 */
void Huff_Init(void)
{
    huffqueue_t queue;
    huffnode_t *node;
    int     i;

    // Initialize the priority queue that holds the remaining nodes.
    queue.count = 0;
    for(i = 0; i < 256; ++i)
    {
        // These are the leaves of the tree.
        node = (huffnode_t*) M_Calloc(sizeof(huffnode_t));
        node->freq = freqs[i];
        node->value = i;
        Huff_QueueInsert(&queue, node);
    }

    // Build the tree.
    for(i = 0; i < 255; ++i)
    {
        node = (huffnode_t*) M_Calloc(sizeof(huffnode_t));
        node->left = Huff_QueueExtract(&queue);
        node->right = Huff_QueueExtract(&queue);
        node->freq = node->left->freq + node->right->freq;
        Huff_QueueInsert(&queue, node);
    }

    // The root is the last node left in the queue.
    huffRoot = Huff_QueueExtract(&queue);

    // Fill in the code lookup table.
    Huff_BuildLookup(huffRoot, 0, 0);

    // Allocate memory for the encoding/decoding buffer.
    Huff_CheckBuffer(&huffEnc, 0x10000);
    Huff_CheckBuffer(&huffDec, 0x10000);

    if(ArgExists("-huffcodes"))
    {
        double  realBits = 0;
        double  huffBits = 0;

        Con_Printf("Huffman codes:\n");
        for(i = 0; i < 256; ++i)
        {
            uint    k;

            realBits += freqs[i] * 8;
            huffBits += freqs[i] * huffCodes[i].length;
            Con_Printf("%03i: (%07i) ", i, (int) (6e6 * freqs[i]));
            for(k = 0; k < huffCodes[i].length; ++k)
            {
                Con_Printf("%c", huffCodes[i].code & (1 << k) ? '1' : '0');
            }
            Con_Printf("\n");
        }
        Con_Printf("realbits=%f, huffbits=%f (%f%%)\n", realBits, huffBits,
                   huffBits / realBits * 100);
    }
}

/**
 * Recursively frees the node and its subtree.
 */
static void Huff_DestroyNode(huffnode_t *node)
{
    if(node)
    {
        Huff_DestroyNode(node->left);
        Huff_DestroyNode(node->right);
        M_Free(node);
    }
}

/**
 * Free the buffer.
 */
static void Huff_DestroyBuffer(huffbuffer_t *buffer)
{
    M_Free(buffer->data);
    memset(buffer, 0, sizeof(*buffer));
}

/**
 * Free all resources allocated here.
 */
void Huff_Shutdown(void)
{
    Huff_DestroyNode(huffRoot);
    huffRoot = NULL;

    // Free the encoding/decoding buffer.
    Huff_DestroyBuffer(&huffEnc);
    Huff_DestroyBuffer(&huffDec);
}

/**
 * Encode the data using Huffman codes. Returns a pointer to the encoded
 * block of bits. The number of bytes in the encoded data is returned
 * in 'encodedSize'.
 */
void *Huff_Encode(byte *data, size_t size, size_t *encodedSize)
{
    size_t          i;
    uint            code;
    int             remaining, fits;
    byte           *out, bit;

    // The encoded message is never twice the original size
    // (longest codes are currently 11 bits).
    Huff_CheckBuffer(&huffEnc, 2 * size);

    // First three bits of the encoded data contain the number of bits (-1)
    // in the last byte of the encoded data. It's written when we have
    // finished the encoding.
    bit = 3;

    // Clear the first byte of the result.
    out = huffEnc.data;
    *out = 0;

    for(i = 0; i < size; ++i)
    {
        remaining = huffCodes[data[i]].length;
        code = huffCodes[data[i]].code;

        while(remaining > 0)
        {
            fits = 8 - bit;
            if(fits > remaining)
                fits = remaining;

            // Write the bits that fit the current byte.
            *out |= code << bit;
            code >>= fits;
            remaining -= fits;

            // Advance the bit position.
            bit += fits;
            if(bit == 8)
            {
                bit = 0;
                *++out = 0;
            }
        }
    }

    // If the last byte is empty, back up.
    if(bit == 0)
    {
        out--;
        bit = 8;
    }

    if(encodedSize)
        *encodedSize = out - huffEnc.data + 1;

    // The number of valid bits - 1 in the last byte.
    huffEnc.data[0] |= bit - 1;

/*#if _DEBUG
{
// Test decode.
byte           *backup = M_Malloc(huffDec.size);
size_t          decsize;

memcpy(backup, huffDec.data, huffDec.size);
Huff_Decode(huffEnc.data, *encodedSize, &decsize);
if(decsize != size || memcmp(huffDec.data, data, size))
{
    Con_Error("Huff_Encode: Data corruption!\n");
}
memcpy(huffDec.data, backup, huffDec.size);
M_Free(backup);
}
#endif*/

    return huffEnc.data;
}

/**
 * Decode using the Huffman tree. Returns a pointer to the decoded data.
 */
byte *Huff_Decode(void *data, size_t size, size_t *decodedSize)
{
    huffnode_t     *node;
    size_t          outBytes = 0;
    byte           *in = (byte*) data, bit = 3, lastByteBits;
    byte           *lastIn = in + size - 1;

    // The first three bits contain the number of valid bits in
    // the last byte.
    lastByteBits = (*in & 7) + 1;

    // Start from the root node.
    node = huffRoot;

    while(in < lastIn || bit < lastByteBits)
    {
        // Go left or right?
        if(*in & (1 << bit))
        {
            node = node->right;
        }
        else
        {
            node = node->left;
        }

        // Did we arrive at a leaf?
        if(!node->left && !node->right)
        {
            // This node represents a value.
            huffDec.data[outBytes++] = node->value;

            // Should we allocate more memory?
            if(outBytes == huffDec.size)
            {
                Huff_CheckBuffer(&huffDec, 2 * huffDec.size);
            }

            // Back to the root.
            node = huffRoot;
        }

        // Advance bit position.
        if(++bit == 8)
        {
            bit = 0;
            ++in;

            // Out of buffer?
            if(in > lastIn)
                break;
        }
    }

    if(decodedSize)
        *decodedSize = outBytes;
    return huffDec.data;
}
