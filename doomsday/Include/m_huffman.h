//===========================================================================
// M_HUFFMAN.H
//===========================================================================
#ifndef __DOOMSDAY_MISC_HUFFMAN_H__
#define __DOOMSDAY_MISC_HUFFMAN_H__

void	Huff_Init(void);
void	Huff_Shutdown(void);
void*	Huff_Encode(byte *data, uint size, uint *encodedSize);
byte*	Huff_Decode(void *data, uint size, uint *decodedSize);

#endif 