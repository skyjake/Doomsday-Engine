#ifndef __DOOMSDAY_NETMESSAGE_H__
#define __DOOMSDAY_NETMESSAGE_H__

void Msg_Begin(int type);
void Msg_WriteByte(byte b);
void Msg_WriteShort(short w);
void Msg_WritePackedShort(short w);
void Msg_WriteLong(int l);
void Msg_Write(const void *src, int len);
byte Msg_ReadByte();
short Msg_ReadShort();
short Msg_ReadPackedShort();
int Msg_ReadLong();
void Msg_Read(void *dest, int len);
int Msg_Offset();
void Msg_SetOffset(int offset);
int Msg_MemoryLeft();
boolean Msg_End();

#endif