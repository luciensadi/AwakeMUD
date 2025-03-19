#ifndef __gmcp_h__
#define __gmcp_h__

typedef struct descriptor_data descriptor_t;

#define MAX_GMCP_SIZE 4096

/******************************************************************************
 GMCP functions.
 ******************************************************************************/
void SendGMCP( descriptor_t *apDescriptor, const char *module, const char *apData );
void SendGMCPCharInfo( struct char_data * ch );
void SendGMCPRoomInfo( struct char_data *ch, struct room_data *room );
void SendGMCPCoreSupports ( descriptor_t *apDescriptor );
void SendGMCPCharStatus( struct char_data * ch );

void ParseGMCP( descriptor_t *apDescriptor, const char *apData );
#endif
